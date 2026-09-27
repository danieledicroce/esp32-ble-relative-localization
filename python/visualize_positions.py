import json
import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

SERIAL_PORT = "/dev/ttyUSB0"  # da adattare alla propria porta seriale
BAUD_RATE = 115200
STALE_THRESHOLD = 5  # letture consecutive con la stessa distanza , oltre le quali è considerata non aggiornata

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
ser.readline()  # scarta la prima lettura

fig, ax = plt.subplots()

NODE_COLORS = {0: "tab:blue", 1: "tab:red", 2: "tab:green"}
last_device_distance = None
last_good_position = (0.0, 0.0)
stale_count = 0

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

def draw_position_labels(ax, xs, ys, ids, device_x, device_y, device_color, device_label, device_visible):
    for i, (x, y, node_id) in enumerate(zip(xs, ys, ids)):
        color = NODE_COLORS.get(node_id, "tab:gray")
        ax.text(0.02, 0.98 - 0.05 * i, f"Node {node_id}: ({x:.2f}, {y:.2f})",
                transform=ax.transAxes, color=color, fontsize=9, verticalalignment="top")
    if device_visible:
        ax.text(0.02, 0.98 - 0.05 * len(ids), f"{device_label}: ({device_x:.2f}, {device_y:.2f})",
                transform=ax.transAxes, color=device_color, fontsize=9, verticalalignment="top")

def update(frame):
    global last_device_distance, last_good_position, stale_count

    data = read_positions()
    if data is None:
        return

    # print("DEBUG:", data)

    xs = [node["x"] for node in data["n"]]
    ys = [node["y"] for node in data["n"]]
    ids = [node["i"] for node in data["n"]]

    ax.clear()
    draw_nodes(ax, xs, ys, ids)

    device_x = data["d"]["x"]
    device_y = data["d"]["y"]
    device_distance = data["d"]["r"]

    if device_distance == last_device_distance:
        stale_count += 1
    else:
        stale_count = 0
        last_good_position = (device_x, device_y)
    last_device_distance = device_distance

    device_visible = device_x != 0.0 or device_y != 0.0
    device_color = None
    device_label = None
    draw_x, draw_y = device_x, device_y

    if device_visible:
        if stale_count >= STALE_THRESHOLD:
            draw_x, draw_y = last_good_position
            device_color = "tab:gray"
            device_label = "Last position"
        else:
            device_color = "magenta"
            device_label = "Device"

        ax.scatter(draw_x, draw_y, c=device_color, marker="x", s=100)
        ax.annotate(device_label, (draw_x, draw_y))

    draw_position_labels(ax, xs, ys, ids, draw_x, draw_y, device_color, device_label, device_visible)

    ax.set_title("Nodes position")

try:
    animation = FuncAnimation(fig, update, interval=500, cache_frame_data=False)
    plt.show()
finally:
    ser.close()