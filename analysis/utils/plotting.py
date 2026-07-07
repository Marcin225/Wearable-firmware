import os
import matplotlib.pyplot as plt


def plot_raw_vs_filtered(debug_df, title, out_path=None):
    plt.figure(figsize=(14, 6))

    plt.plot(debug_df["time_s"], debug_df["ir_raw"], label="IR raw", alpha=0.6)
    plt.plot(debug_df["time_s"], debug_df["ir_filtered"], label="IR filtered", linewidth=1.5)

    plt.title(title)
    plt.xlabel("Time [s]")
    plt.ylabel("Signal value")
    plt.legend()
    plt.grid(True)

    if out_path:
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        plt.savefig(out_path, dpi=150, bbox_inches="tight")
    else:
        plt.show()

    plt.close()


def plot_red_raw_vs_filtered(debug_df, title, out_path=None):
    plt.figure(figsize=(14, 6))

    plt.plot(debug_df["time_s"], debug_df["red_raw"], label="RED raw", alpha=0.6)
    plt.plot(debug_df["time_s"], debug_df["red_filtered"], label="RED filtered", linewidth=1.5)

    plt.title(title)
    plt.xlabel("Time [s]")
    plt.ylabel("Signal value")
    plt.legend()
    plt.grid(True)

    if out_path:
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        plt.savefig(out_path, dpi=150, bbox_inches="tight")
    else:
        plt.show()

    plt.close()