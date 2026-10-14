"""Esegue la simulazione senza grafica e stampa l'errore di posizione.

Uso:  python sim/run_sim.py [--robots 10] [--cycles 40] [--sketch broadcast_observer|broadcast]
                                     [--motion waypoints|circle|line] [--noise 3.0] [--loss 0.0] [--seed 1]
"""
import argparse
import math
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from sim.sim import Simulation  # noqa: E402


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--robots", type=int, default=None)
    p.add_argument("--cycles", type=int, default=40)
    p.add_argument("--sketch", default="broadcast_observer", choices=["broadcast_observer", "broadcast"])
    p.add_argument("--motion", default="waypoints", choices=["waypoints", "circle", "line"])
    p.add_argument("--noise", type=float, default=3.0)
    p.add_argument("--loss", type=float, default=0.0)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--viewer", default="anchor0")
    a = p.parse_args()

    sim = Simulation(a.sketch, a.robots, a.motion, noise_db=a.noise, packet_loss=a.loss, seed=a.seed, viewer=a.viewer)
    errors, located = [], []
    for cycle in range(a.cycles):
        data = sim.step()
        truth = {r["i"]: (r["x"], r["y"]) for r in data["t"]}
        located.append(len(data["d"]))
        if cycle >= 5:   # i primi cicli servono al filtro di Kalman per avviarsi
            errors += [math.dist((r["x"], r["y"]), truth[r["i"]]) for r in data["d"]]

    print(f"robot: {len(sim.robots)} | cicli: {a.cycles} ({a.cycles * 3} s simulati) | rumore RSSI: {a.noise} dB | perdita pacchetti: {a.loss}")
    print(f"robot localizzati per ciclo: media {statistics.mean(located):.1f} su {len(sim.robots)}")
    if errors:
        errors.sort()
        print(f"errore di posizione: mediano {statistics.median(errors):.2f} m | 90° percentile {errors[int(0.9 * (len(errors) - 1))]:.2f} m | massimo {errors[-1]:.2f} m")


if __name__ == "__main__":
    main()