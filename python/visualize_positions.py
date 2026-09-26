import json
import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

SERIAL_PORT = "/dev/ttyUSB0"  # da adattare alla propria porta seriale
BAUD_RATE = 115200

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

fig, ax = plt.subplots()

NODE_COLORS = {0: "tab:blue", 1: "tab:red", 2: "tab:green"}

def read_positions():
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    if not line:
        return None
    try:
        return json.loads(line)
    except json.JSONDecodeError:
        return None

def draw_nodes(ax, xs, ys, ids):
    colors = [NODE_COLORS.get(node_id, "tab:gray") for node_id in ids]
    ax.scatter(xs, ys, c=colors)
    for x, y, node_id in zip(xs, ys, ids):
        ax.annotate(f"Node {node_id}", (x, y))
    ax.set_xlabel("x (m)")
    ax.set_ylabel("y (m)")
    ax.axis("equal")

def update(frame):
    data = read_positions()
    if data is None:
        return

    xs = [node["x"] for node in data["nodes"]]
    ys = [node["y"] for node in data["nodes"]]
    ids = [node["id"] for node in data["nodes"]]

    ax.clear()
    draw_nodes(ax, xs, ys, ids)

    device_x = data["device"]["x"]
    device_y = data["device"]["y"]
    if device_x != 0.0 or device_y != 0.0:
        ax.scatter(device_x, device_y, c="magenta", marker="x", s=100)
        ax.annotate("Device", (device_x, device_y))

    ax.set_title("Nodes position")

animation = FuncAnimation(fig, update, interval=500, cache_frame_data=False)
plt.show()