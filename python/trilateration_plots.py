import math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle

NODE_COLORS = {0: "tab:blue", 1: "tab:red", 2: "tab:green"}


def position_of_node2(d01, d02, d12):
    """Stessa formula di computePositions(): Nodo 0 in (0,0), Nodo 1 in (d01,0), y2 >= 0."""
    x2 = (d02 * d02 - d12 * d12 + d01 * d01) / (2 * d01)
    y2 = math.sqrt(max(d02 * d02 - x2 * x2, 0.0))
    return x2, y2


def plot_case(d01, d02, d12, title, filename, right_angle=False):
    x2, y2 = position_of_node2(d01, d02, d12)
    xs = [0.0, d01, x2]
    ys = [0.0, 0.0, y2]

    fig, ax = plt.subplots(figsize=(6, 5.2))

    # circonferenze: centro Nodo 0 raggio d02, centro Nodo 1 raggio d12
    ax.add_patch(Circle((0, 0), d02, fill=False, color="tab:blue", lw=1, alpha=0.6))
    ax.add_patch(Circle((d01, 0), d12, fill=False, color="tab:red", lw=1, alpha=0.6))

    # triangolo e soluzione speculare (scartata)
    ax.plot([0, d01, x2, 0], [0, 0, y2, 0], color="black", lw=1.2)
    ax.plot([x2, x2], [y2, -y2], color="gray", lw=1, ls="--")
    ax.scatter([x2], [-y2], facecolors="none", edgecolors="gray", s=60, zorder=3)
    ax.annotate("Mirror solution (discarded)", (x2, -y2), textcoords="offset points",
                xytext=(8, -4), color="gray", fontsize=9)

    # nodi
    ax.scatter(xs, ys, c=[NODE_COLORS[i] for i in range(3)], zorder=4)
    for i, (x, y) in enumerate(zip(xs, ys)):
        ax.annotate(f"Node {i}", (x, y), textcoords="offset points", xytext=(6, 6))

    # etichette delle distanze sui lati
    ax.annotate(r"$d_{01}$", (d01 / 2, 0), textcoords="offset points", xytext=(0, -14), ha="center")
    ax.annotate(r"$d_{02}$", (x2 / 2, y2 / 2), textcoords="offset points", xytext=(-16, 2))
    ax.annotate(r"$d_{12}$", ((x2 + d01) / 2, y2 / 2), textcoords="offset points", xytext=(6, 4))

    if right_angle:
        s = 0.07
        ax.plot([s, s, 0], [0, s, s], color="black", lw=1)

    # coordinate in alto a sinistra, con i colori dei nodi (come nello script di visualizzazione)
    for i, (x, y) in enumerate(zip(xs, ys)):
        ax.text(0.02, 0.98 - 0.05 * i, f"Node {i}: ({x:.2f}, {y:.2f})",
                transform=ax.transAxes, color=NODE_COLORS[i], fontsize=9, verticalalignment="top")

    ax.set_xlabel("x (m)")
    ax.set_ylabel("y (m)")
    ax.axis("equal")
    ax.set_title(title)
    fig.tight_layout()
    fig.savefig(filename, dpi=200)
    plt.close(fig)
    print(filename, "x2=%.3f y2=%.3f" % (x2, y2))


# (a) caso generale
plot_case(1.20, 0.92, 1.10, "Trilateration: general case", "trilateration_general.png")
# (b) Nodo 2 sulla verticale del Nodo 0 (triangolo rettangolo 3-4-5)
plot_case(1.20, 0.90, 1.50, "Trilateration: right triangle", "trilateration_right_triangle.png", right_angle=True)