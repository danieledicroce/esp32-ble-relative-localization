import json
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.ticker import MultipleLocator
from PIL import Image

SOURCE = "simulator"                   # "serial" = schede vere; "simulator" = simulazione (cartella sim)
SERIAL_PORT = "/dev/ttyUSB0"        # da adattare alla propria porta seriale
BAUD_RATE = 115200

# Opzioni della simulazione (usate solo con SOURCE = "simulator"), vedi sim/sim.py
SIM_OPTIONS = dict(
    robot_sketch="broadcast_observer",  # "broadcast_observer" o "broadcast": sketch caricato sui robot
    n_robots=3,                  # None = tutti i robot degli sketch (NUM_DEVICES)
    motion="waypoints",             # "waypoints", "circle" oppure "line"
    speed=0.3,                      # velocita' dei robot in m/s
    noise_db=3.0,                   # rumore dell'RSSI in dB
    packet_loss=0.0,                # probabilita' di perdere un pacchetto (0-1)
    seed=100,                         # stesso numero = stessa simulazione
    viewer="anchor0",               # scheda di cui si legge il JSON: "anchor0", "robot16", ...
)
SIM_SPEED = 5.0                     # simulatore: 1 = tempo reale (un ciclo ogni 3 s), 2 = doppia velocita', 0.5 = meta'
SHOW_TRUTH = True                   # simulatore: mostra anche la posizione vera dei robot (croce)

# Immagine di sfondo (mappa, planimetria, strada). None = nessuno sfondo.
BACKGROUND_IMAGE = "assets/example_map.png"             # ad esempio "mappa.png"
METERS_PER_PIXEL = 0.01             # scala dell'immagine: 0.01 = 1 pixel e' 1 cm
ORIGIN_PIXEL = (100, 500)               # pixel (x, y), contato dall'angolo in alto a sinistra,
                                        # che corrisponde al punto (0, 0) del sistema di riferimento delle ancore
MAX_IMAGE_SIDE = 1500               # l'immagine viene ridotta se piu' grande (la scala si adegua da sola)
VIEW_LIMITS = None                  # None = tutta l'immagine; oppure (xmin, xmax, ymin, ymax) in metri

SHOW_GRID = True                    # griglia sopra lo sfondo
GRID_STEP = 0.1                     # distanza tra le linee sottili della griglia, in metri (0.1 = decimetri)
GRID_LABEL_STEP = 1.0               # distanza tra le linee marcate, che hanno anche l'etichetta sugli assi

TRAIL_LENGTH = 5                    # numero di posizioni passate mostrate per ogni robot (0 = nessuna)

NODE_COLORS = {0: "tab:blue", 1: "tab:red", 2: "tab:green"}
ROBOT_COLORS = plt.get_cmap("tab10").colors

ser = None
simulation = None
fig, ax = plt.subplots()
dynamic_artists = []   # elementi ridisegnati a ogni lettura (lo sfondo viene disegnato una volta sola)
trails = {}            # id robot -> lista delle ultime posizioni


def load_background():
    image = Image.open(BACKGROUND_IMAGE).convert("RGB")
    width, height = image.size
    scale = METERS_PER_PIXEL

    longest = max(width, height)
    if longest > MAX_IMAGE_SIDE:
        factor = MAX_IMAGE_SIDE / longest
        image = image.resize((round(width * factor), round(height * factor)))
        scale = METERS_PER_PIXEL / factor   # ogni pixel dell'immagine ridotta vale piu' metri
        width, height = image.size
        origin_x = ORIGIN_PIXEL[0] * factor
        origin_y = ORIGIN_PIXEL[1] * factor
    else:
        origin_x, origin_y = ORIGIN_PIXEL

    # coordinate in metri degli angoli dell'immagine: sinistra, destra, basso, alto
    extent = (-origin_x * scale, (width - origin_x) * scale,
              -(height - origin_y) * scale, origin_y * scale)
    ax.imshow(image, extent=extent, zorder=0)
    return extent


def read_positions():
    if SOURCE == "simulator":
        return simulation.step()
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    if not line:
        return None
    try:
        return json.loads(line)
    except json.JSONDecodeError:
        return None


def add(artist):
    dynamic_artists.append(artist)


def draw_anchors(nodes):
    xs = [node["x"] for node in nodes]
    ys = [node["y"] for node in nodes]
    colors = [NODE_COLORS.get(node["i"], "tab:gray") for node in nodes]
    add(ax.scatter(xs, ys, c=colors, s=80, marker="s", edgecolors="black", zorder=3))
    for node in nodes:
        add(ax.annotate(f"Anchor {node['i']}", (node["x"], node["y"]), zorder=4))


def draw_robots(robots):
    for robot in robots:
        robot_id = robot["i"]
        color = ROBOT_COLORS[robot_id % len(ROBOT_COLORS)]

        history = trails.setdefault(robot_id, [])
        history.append((robot["x"], robot["y"]))
        del history[:-(TRAIL_LENGTH + 1)]

        if TRAIL_LENGTH > 0 and len(history) > 1:
            trail_xs, trail_ys = zip(*history)
            add(ax.plot(trail_xs, trail_ys, color=color, linewidth=1, alpha=0.6, zorder=2)[0])

        add(ax.scatter(robot["x"], robot["y"], color=color, s=60, edgecolors="black", zorder=3))
        add(ax.annotate(f"R{robot_id}", (robot["x"], robot["y"]), zorder=4))


def draw_truth(truth):
    for robot in truth:
        color = ROBOT_COLORS[robot["i"] % len(ROBOT_COLORS)]
        add(ax.scatter(robot["x"], robot["y"], color=color, marker="x", s=50, linewidths=1.5, zorder=3))


def draw_position_labels(nodes, robots):
    lines = [(f"Anchor {node['i']}: ({node['x']:.2f}, {node['y']:.2f})",
              NODE_COLORS.get(node["i"], "tab:gray")) for node in nodes]
    lines += [(f"Robot {robot['i']}: ({robot['x']:.2f}, {robot['y']:.2f})",
               ROBOT_COLORS[robot["i"] % len(ROBOT_COLORS)]) for robot in robots]
    for i, (text, color) in enumerate(lines):
        add(ax.text(1.02, 1.0 - 0.045 * i, text, transform=ax.transAxes, color=color,
                    fontsize=8, verticalalignment="top", clip_on=False))


def update(frame):
    data = read_positions()
    if data is None:
        return

    for artist in dynamic_artists:
        artist.remove()
    dynamic_artists.clear()

    draw_anchors(data["n"])
    draw_robots(data["d"])
    if SHOW_TRUTH and "t" in data:
        draw_truth(data["t"])
    draw_position_labels(data["n"], data["d"])


def setup_axes(extent):
    fig.subplots_adjust(right=0.74)   # spazio a destra per l'elenco delle posizioni, fuori dal grafico
    ax.set_xlabel("x (m)")
    ax.set_ylabel("y (m)")
    ax.set_title("Robot position")
    ax.set_aspect("equal")
    if SHOW_GRID:
        ax.xaxis.set_major_locator(MultipleLocator(GRID_LABEL_STEP))
        ax.yaxis.set_major_locator(MultipleLocator(GRID_LABEL_STEP))
        ax.grid(True, which="major", color="gray", linewidth=0.8, alpha=0.6)
        if GRID_STEP < GRID_LABEL_STEP:
            ax.xaxis.set_minor_locator(MultipleLocator(GRID_STEP))
            ax.yaxis.set_minor_locator(MultipleLocator(GRID_STEP))
            ax.grid(True, which="minor", color="gray", linewidth=0.3, alpha=0.4)
        ax.set_axisbelow("line")   # la griglia sta sopra l'immagine e sotto punti e scie
    if VIEW_LIMITS is not None:
        ax.set_xlim(VIEW_LIMITS[0], VIEW_LIMITS[1])
        ax.set_ylim(VIEW_LIMITS[2], VIEW_LIMITS[3])
    elif extent is not None:
        ax.set_xlim(extent[0], extent[1])
        ax.set_ylim(extent[2], extent[3])
    else:
        ax.set_xlim(-1, 7)
        ax.set_ylim(-1, 5)


if __name__ == "__main__":
    extent = load_background() if BACKGROUND_IMAGE else None
    setup_axes(extent)

    if SOURCE == "simulator":
        from sim.sim import Simulation
        simulation = Simulation(**SIM_OPTIONS)
    else:
        import serial
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        ser.readline()  # scarta la prima lettura, che puo' contenere un frammento di riga gia' in corso

    try:
        interval = 3000 / SIM_SPEED if SOURCE == "simulator" else 500   # millisecondi tra un aggiornamento e il successivo
        animation = FuncAnimation(fig, update, interval=interval, cache_frame_data=False)
        plt.show()
    finally:
        if ser is not None:
            ser.close()