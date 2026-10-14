"""Simulatore: fa girare gli sketch veri (compilati per PC) con ancore e robot che si scambiano pacchetti BLE finti.

Ogni chiamata a Simulation.step() simula un ciclo di 3 secondi:
  1. i robot si muovono;
  2. ogni scheda che scansiona riceve i pacchetti degli altri (al massimo uno per trasmettitore per ciclo,
     come il filtro sui duplicati dello scanner) con l'RSSI calcolato dal modello di path loss;
  3. ogni scheda esegue loop() (calcola, aggiorna il payload e stampa il JSON sulla seriale).
Restituisce il JSON stampato dalla scheda scelta come "osservatore" (di default l'ancora 0), piu' le posizioni vere.
"""
import ctypes
import json
import math
import os
import random
import re

from . import build

CYCLE_SECONDS = 3.0

# Modello radio del "mondo" (non dello sketch). Se uguali ai parametri degli sketch il modello e' perfetto;
# cambiandoli si simula un ambiente diverso da quello usato nella taratura.
WORLD_RSSI_AT_1M = -56.0
WORLD_PATH_LOSS_EXPONENT = 3.0


def read_defines(path):
    text = open(path, encoding="utf-8").read()
    defines = {}
    for name in ("NUM_FIXED_NODES", "NUM_DEVICES", "DEVICE_ID_BASE"):
        defines[name] = int(re.search(rf"^#define\s+{name}\s+(\d+)", text, flags=re.M).group(1))

    def array(name):
        body = re.search(rf"{name}\[[^\]]*\]\s*=\s*\{{([^}}]*)\}}", text).group(1)
        return [float(v) for v in body.split(",")]

    defines["NODE_X"] = array("NODE_X")
    defines["NODE_Y"] = array("NODE_Y")
    return defines


class Board:
    """Una scheda: una copia della libreria dello sketch, con il proprio stato."""

    def __init__(self, kind, index, node_id):
        self.kind = kind
        self.node_id = node_id
        self.lib = ctypes.CDLL(build.instance_library(kind, index))
        self.lib.sim_rx.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
        self.lib.sim_payload.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self.lib.sim_serial.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self.lib.sim_start(node_id)
        self.last_json = None

    def receive(self, payload, rssi):
        self.lib.sim_rx(payload, len(payload), rssi)

    def payload(self):
        buffer = ctypes.create_string_buffer(64)
        n = self.lib.sim_payload(buffer, 64)
        return buffer.raw[:n]

    def run_loop(self):
        self.lib.sim_loop()
        buffer = ctypes.create_string_buffer(65536)
        n = self.lib.sim_serial(buffer, 65536)
        for line in buffer.raw[:n].decode().splitlines():
            if line.startswith("{"):
                self.last_json = json.loads(line)


class Robot:
    def __init__(self, robot_id, area, motion, speed, rng):
        self.id = robot_id
        self.area = area
        self.motion = motion
        self.speed = speed
        self.rng = rng
        xmin, xmax, ymin, ymax = area
        self.x = rng.uniform(xmin, xmax)
        self.y = rng.uniform(ymin, ymax)
        self.target = (self.x, self.y)
        self.direction = rng.choice((-1, 1))
        self.cx, self.cy = (xmin + xmax) / 2, (ymin + ymax) / 2
        self.radius = rng.uniform(0.4, 1.0) * min(xmax - xmin, ymax - ymin) / 2
        self.angle = rng.uniform(0, 2 * math.pi)
        self.row = self.y

    def move(self, dt):
        xmin, xmax, ymin, ymax = self.area
        if self.motion == "circle":
            self.angle += self.speed / self.radius * dt
            self.x = self.cx + self.radius * math.cos(self.angle)
            self.y = self.cy + self.radius * math.sin(self.angle)
        elif self.motion == "line":
            self.x += self.direction * self.speed * dt
            if self.x < xmin or self.x > xmax:
                self.x = min(max(self.x, xmin), xmax)
                self.direction = -self.direction
            self.y = self.row
        else:  # "waypoints": va verso un punto casuale, poi ne sceglie un altro
            dx, dy = self.target[0] - self.x, self.target[1] - self.y
            distance = math.hypot(dx, dy)
            step = self.speed * dt
            if distance <= step:
                self.x, self.y = self.target
                self.target = (self.rng.uniform(xmin, xmax), self.rng.uniform(ymin, ymax))
            else:
                self.x += dx / distance * step
                self.y += dy / distance * step


class Simulation:
    def __init__(self, robot_sketch="broadcast_observer", n_robots=None, motion="waypoints", speed=0.3,
                 noise_db=3.0, packet_loss=0.0, seed=1, viewer="anchor0", margin=0.3):
        """robot_sketch: "broadcast_observer" o "broadcast" (un solo tipo di robot per volta).
        viewer: scheda di cui si legge il JSON: "anchor0", "anchor1", ... oppure "robot16", ...
        """
        if robot_sketch == "observer":
            robot_sketch = "broadcast_observer"
        build.build_all(("anchor", robot_sketch))
        cfg = read_defines(build.SKETCHES["anchor"])
        self.cfg = cfg
        self.anchors_xy = list(zip(cfg["NODE_X"], cfg["NODE_Y"]))
        self.noise_db = noise_db
        self.packet_loss = packet_loss
        self.rng = random.Random(seed)
        self.time = 0.0

        n = cfg["NUM_DEVICES"] if n_robots is None else n_robots
        if n > cfg["NUM_DEVICES"]:
            raise ValueError(f"n_robots ({n}) supera NUM_DEVICES ({cfg['NUM_DEVICES']}) degli sketch")

        xs, ys = cfg["NODE_X"], cfg["NODE_Y"]
        area = (min(xs) + margin, max(xs) - margin, min(ys) + margin, max(ys) - margin)
        self.robots = [Robot(cfg["DEVICE_ID_BASE"] + i, area, motion, speed, self.rng) for i in range(n)]

        self.anchors = [Board("anchor", i, i) for i in range(cfg["NUM_FIXED_NODES"])]
        self.robot_boards = [Board(robot_sketch, i, r.id) for i, r in enumerate(self.robots)]
        self.scanning = self.anchors + (self.robot_boards if robot_sketch == "broadcast_observer" else [])
        self.viewer = self._find_viewer(viewer)

    def _find_viewer(self, name):
        if name.startswith("anchor"):
            return self.anchors[int(name[6:])]
        if name.startswith("robot"):
            for board in self.robot_boards:
                if board.node_id == int(name[5:]):
                    if board.kind != "broadcast_observer":
                        raise ValueError("il robot scelto non stampa il JSON: serve lo sketch observer")
                    return board
        raise ValueError(f"viewer '{name}' non valido")

    def _position(self, board):
        if board in self.anchors:
            return self.anchors_xy[board.node_id]
        robot = next(r for r in self.robots if r.id == board.node_id)
        return (robot.x, robot.y)

    def _rssi(self, distance):
        distance = max(distance, 0.1)
        value = WORLD_RSSI_AT_1M - 10 * WORLD_PATH_LOSS_EXPONENT * math.log10(distance)
        return round(value + self.rng.gauss(0, self.noise_db))

    def step(self):
        for robot in self.robots:
            robot.move(CYCLE_SECONDS)
        self.time += CYCLE_SECONDS

        transmitters = self.anchors + self.robot_boards
        # i pacchetti vengono letti prima che le schede li aggiornino: ogni scheda trasmette quello del ciclo precedente
        packets = [(board, board.payload(), self._position(board)) for board in transmitters]
        for listener in self.scanning:
            lx, ly = self._position(listener)
            for sender, payload, (sx, sy) in packets:
                if sender is listener or self.rng.random() < self.packet_loss:
                    continue
                listener.receive(payload, self._rssi(math.hypot(lx - sx, ly - sy)))
        for board in self.anchors + self.robot_boards:
            board.run_loop()

        data = dict(self.viewer.last_json) if self.viewer.last_json else {"n": [], "d": []}
        data["t"] = [{"i": r.id, "x": round(r.x, 2), "y": round(r.y, 2)} for r in self.robots]
        return data