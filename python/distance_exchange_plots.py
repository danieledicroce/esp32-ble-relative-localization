import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

NODE_COLORS = {0: "tab:blue", 1: "tab:red", 2: "tab:green", 3: "tab:orange"}
SPAN_X, SPAN_Y = 1.87, 1.2   # stessa scala per entrambe le figure


def plot_view(positions, title, filename, label_offsets=None, label_t=None):
    """Distanze misurate direttamente dal Nodo 0 (linea continua) e distanze
    che devono essergli comunicate dagli altri nodi (linea tratteggiata)."""
    n = len(positions)
    fig, ax = plt.subplots(figsize=(4.3, 3.0))

    for i in range(n):
        for j in range(i + 1, n):
            (xi, yi), (xj, yj) = positions[i], positions[j]
            measured = (i == 0)
            ax.plot([xi, xj], [yi, yj],
                    color="tab:blue" if measured else "gray",
                    lw=1.8 if measured else 1.2,
                    ls="-" if measured else "--",
                    alpha=1.0 if measured else 0.8, zorder=1)
            f = (label_t or {}).get((i, j), 0.5)
            mx, my = xi + f * (xj - xi), yi + f * (yj - yi)
            dx, dy = (label_offsets or {}).get((i, j), (0, 0))
            ax.text(mx + dx, my + dy, rf"$d_{{{i}{j}}}$", ha="center", va="center", fontsize=9,
                    color="tab:blue" if measured else "dimgray",
                    bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none", alpha=0.9), zorder=2)

    for i, (x, y) in enumerate(positions):
        if i == 0:
            ax.scatter([x], [y], s=260, facecolors="none", edgecolors="tab:blue", lw=1.5, zorder=3)
        ax.scatter([x], [y], c=NODE_COLORS[i], s=60, zorder=4)
        ax.annotate(f"Node {i}", (x, y), textcoords="offset points", xytext=(0, 12),
                    ha="center", fontsize=10)

    # limiti centrati sui nodi, con la stessa scala per entrambe le figure
    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    ax.set_xlim(cx - SPAN_X / 2, cx + SPAN_X / 2)
    ax.set_ylim(cy - SPAN_Y / 2, cy + SPAN_Y / 2)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title(title, fontsize=11)
    fig.tight_layout()
    fig.savefig(filename, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(filename)


OFFS = {(0, 1): (0, -0.07), (0, 2): (-0.07, 0.03), (1, 2): (0.07, 0.03)}

# (a) N = 3: a Node 0 manca solo d12
plot_view([(0.0, 0.0), (1.2, 0.0), (0.5, 0.9)], "N = 3", "distances_exchange_n3.png",
          label_offsets=OFFS)

# (b) N = 4: stessi tre nodi di (a) piu' un quarto nodo
plot_view([(0.0, 0.0), (1.2, 0.0), (0.5, 0.9), (1.6, 0.95)], "N = 4", "distances_exchange_n4.png",
          label_offsets=OFFS, label_t={(0, 3): 0.25, (1, 2): 0.7})