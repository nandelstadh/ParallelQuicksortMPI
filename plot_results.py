#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

PIVOT_LABELS = {
    1: "root median",
    2: "mean of medians",
    3: "median of medians",
}


def pivot_label(h: int) -> str:
    return PIVOT_LABELS.get(h, f"h={h}")


def load_rows(csv_file: Path) -> list[dict[str, float | int]]:
    rows: list[dict[str, float | int]] = []
    with csv_file.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        required = {"n", "p", "h", "time"}
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            raise ValueError(f"{csv_file} must contain columns: n,p,h,time")

        for row in reader:
            rows.append(
                {
                    "n": int(row["n"]),
                    "p": int(row["p"]),
                    "h": int(row["h"]),
                    "time": float(row["time"]),
                }
            )
    return rows


def group_by(rows: list[dict[str, float | int]], keys: tuple[str, ...]):
    grouped: dict[tuple[int, ...], list[dict[str, float | int]]] = defaultdict(list)
    for row in rows:
        grouped[tuple(int(row[key]) for key in keys)].append(row)
    for row_group in grouped.values():
        row_group.sort(key=lambda entry: int(entry["p"]))
    return grouped


def finalize_plot(
    rows: list[dict[str, float | int]],
    title: str,
    y_label: str,
    out_path: Path,
    y_limits: tuple[float, float] | None = None,
) -> None:
    processors = sorted({int(row["p"]) for row in rows})
    plt.xticks(processors, processors)
    plt.xlabel("Processors (p)")
    plt.ylabel(y_label)
    plt.title(title)
    plt.grid(True, linestyle="--", alpha=0.4)
    if y_limits is not None:
        plt.ylim(*y_limits)
    plt.legend()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)
    plt.close()


def plot_pivot_comparison(csv_path: Path, out_path: Path, title: str) -> None:
    rows = load_rows(csv_path)
    grouped = group_by(rows, ("h",))

    plt.figure(figsize=(8, 5))
    for (h,), row_group in sorted(grouped.items()):
        processors = [int(row["p"]) for row in row_group]
        times = [float(row["time"]) for row in row_group]
        plt.plot(processors, times, marker="o", label=pivot_label(h))

    finalize_plot(rows, title, "Time (s)", out_path)


def plot_strong_scaling(csv_path: Path, out_path: Path) -> None:
    rows = load_rows(csv_path)
    grouped = group_by(rows, ("n", "h"))
    n_values = {int(row["n"]) for row in rows}
    plotted_series = 0
    all_processors: set[int] = set()

    plt.figure(figsize=(8, 5))
    for (n, h), row_group in sorted(grouped.items()):
        baseline = next((float(row["time"]) for row in row_group if int(row["p"]) == 1), None)
        if baseline is None:
            continue

        processors = [int(row["p"]) for row in row_group]
        speedup = [baseline / float(row["time"]) for row in row_group]
        label = pivot_label(h) if len(n_values) == 1 else f"n={n}, {pivot_label(h)}"
        plt.plot(processors, speedup, marker="o", label=label)
        all_processors.update(processors)
        plotted_series += 1

    if plotted_series == 0:
        raise ValueError(f"{csv_path} does not contain any p=1 baseline for strong scaling")

    ideal_processors = sorted(all_processors)
    plt.plot(ideal_processors, ideal_processors, "k--", label="Ideal linear speedup")
    finalize_plot(rows, "Strong scaling", "Speedup (T1 / Tp)", out_path)


def plot_weak_scaling(csv_path: Path, out_path: Path) -> None:
    rows = load_rows(csv_path)
    grouped = group_by(rows, ("h",))
    all_processors = sorted({int(row["p"]) for row in rows})
    if not all_processors:
        raise ValueError(f"{csv_path} does not contain any weak scaling rows")

    initial_p = all_processors[0]
    initial_values = [float(row["time"]) for row in rows if int(row["p"]) == initial_p]
    if not initial_values:
        raise ValueError(f"{csv_path} does not contain an initial weak scaling value")
    initial_time = sum(initial_values) / len(initial_values)

    plt.figure(figsize=(8, 5))
    for (h,), row_group in sorted(grouped.items()):
        processors = [int(row["p"]) for row in row_group]
        times = [float(row["time"]) for row in row_group]
        plt.plot(processors, times, marker="o", label=pivot_label(h))

    plt.plot(
        all_processors,
        [initial_time] * len(all_processors),
        "k--",
        label="Ideal weak scaling (flat)",
    )
    finalize_plot(
        rows,
        "Weak scaling",
        "Time (s)",
        out_path,
        y_limits=(initial_time - 5.0, initial_time + 5.0),
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Create scaling/pivot plots from results CSV files.")
    parser.add_argument("--results-dir", type=Path, default=Path("results"), help="Directory with CSV files")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/plots"),
        help="Directory to write plot images",
    )
    args = parser.parse_args()

    csv_dir = args.results_dir
    out_dir = args.output_dir

    output_files = {
        "pivot_scale.png": out_dir / "pivot_scale.png",
        "pivot_scale_backwards.png": out_dir / "pivot_scale_backwards.png",
        "strong_scaling.png": out_dir / "strong_scaling.png",
        "weak_scaling.png": out_dir / "weak_scaling.png",
    }

    plot_pivot_comparison(
        csv_dir / "pivot_scale.csv",
        output_files["pivot_scale.png"],
        "Pivot strategy comparison (random input)",
    )
    plot_pivot_comparison(
        csv_dir / "pivot_scale_backwards.csv",
        output_files["pivot_scale_backwards.png"],
        "Pivot strategy comparison (backwards input)",
    )
    plot_strong_scaling(csv_dir / "strong_scale.csv", output_files["strong_scaling.png"])
    plot_weak_scaling(csv_dir / "weak_scale.csv", output_files["weak_scaling.png"])

    print("Generated plots:")
    for path in output_files.values():
        print(path)


if __name__ == "__main__":
    main()
