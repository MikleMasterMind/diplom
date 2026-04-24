from dataclasses import dataclass
from pathlib import Path
from typing import Optional
import argparse
import re

from matplotlib.ticker import AutoMinorLocator, FixedLocator

import matplotlib.pyplot as plt
import numpy as np

DATA_DIR = Path("result/data")
GRAPH_DIR = Path("result/graph")
MSS = 1460

# Metric groups that share the same vertical scale
METRIC_GROUPS = {
    "latency": ["rtt", "rtprop", "rtprop_raw"],
    "bandwidth": ["throughput", "goodput", "btlbw", "btlbw_raw"],
}

# All supported metrics (union of all groups)
SUPPORTED_METRICS = set(m for group in METRIC_GROUPS.values() for m in group)

# Metric configuration: label and column index
METRIC_CONFIG = {
    "rtt": {"label": "RTT (s)", "value_col": 1, "context_col": 2},
    "rtprop": {"label": "RTprop (s)", "value_col": 1, "context_col": None},
    "rtprop_raw": {"label": "RTprop raw (s)", "value_col": 1, "context_col": 2},
    "throughput": {"label": "Throughput (Mbps)", "value_col": 1, "context_col": None},
    "goodput": {"label": "Goodput (Mbps)", "value_col": 1, "context_col": None},
    "btlbw": {"label": "BtlBw (Mbps)", "value_col": 1, "context_col": 2},
    "btlbw_raw": {"label": "BtlBw raw (Mbps)", "value_col": 1, "context_col": 2},
}


@dataclass
class PlotData:
    data_file: Path
    output_file: Path
    metric_name: str
    num_paths: int
    num_bad_paths: int
    simulation_time: str
    good_data_rate: str
    good_delay: str
    bad_data_rate: str
    bad_delay: str
    time_values: np.ndarray
    plot_values: np.ndarray
    contexts: Optional[np.ndarray]
    header: Optional[str]
    y_label: str
    y_min_positive: float
    y_max: float


def read_trace_file(path: Path, metric_name: str):
    """Read trace file and return time, values, contexts, header."""
    header = None
    with path.open("r", encoding="utf-8") as file:
        for line in file:
            if line.startswith("#"):
                header = line.strip("#").strip()
                continue
            if line.strip():
                break

    data = np.genfromtxt(
        path,
        comments="#",
        dtype=None,
        encoding="utf-8",
        invalid_raise=False,
    )

    if data.size == 0:
        return None

    if data.ndim == 0:
        data = np.array([data])

    names = data.dtype.names
    config = METRIC_CONFIG.get(metric_name, {"value_col": 1, "context_col": None})

    if names is None:
        array = np.asarray(data)
        if array.ndim == 1:
            array = array.reshape(1, -1)
        if array.shape[1] < 2:
            return None

        time_values = array[:, 0].astype(float)
        metric_values = array[:, config["value_col"]].astype(float)
        contexts = array[:, config["context_col"]].astype(str) if config["context_col"] and array.shape[1] > config["context_col"] else None
    else:
        time_values = data["f0"].astype(float)
        metric_values = data[f"f{config['value_col']}"].astype(float)
        contexts = data[f"f{config['context_col']}"].astype(str) if config["context_col"] and len(names) > config["context_col"] else None

    return time_values, metric_values, contexts, header


def build_output_path(data_file: Path, simulation_time: str) -> Path:
    """Build output path for the plot."""
    relative_path = data_file.relative_to(DATA_DIR)
    output_path = GRAPH_DIR / relative_path
    return output_path.with_suffix(".png")


def extract_plot_metadata(data_file: Path) -> tuple[int, int, str, str, str, str, str]:
    """Extract metadata from file path."""
    relative_parts = data_file.relative_to(DATA_DIR).parts

    num_paths = 1
    num_bad_paths = 0
    simulation_time = "10"
    good_data_rate = "unknown"
    good_delay = "unknown"
    bad_data_rate = "unknown"
    bad_delay = "unknown"

    for part in relative_parts:
        if part.endswith("-numPaths"):
            try:
                num_paths = int(part.split("-", maxsplit=1)[0])
            except ValueError:
                pass
        elif part.endswith("-numBadPaths"):
            try:
                num_bad_paths = int(part.split("-", maxsplit=1)[0])
            except ValueError:
                pass
        elif part.endswith("-simulationTime"):
            simulation_time = part.split("-", maxsplit=1)[0]
        elif "goodDataRate" in part and "goodDelay" in part:
            # Extract goodDataRate and goodDelay from directory name
            # Format: XMbps-goodDataRateYms-goodDelay
            try:
                # Extract goodDataRate (e.g., "5Mbps" from "5Mbps-goodDataRate10ms-goodDelay")
                good_data_rate = part.split("-goodDataRate")[0]
                # Extract goodDelay (e.g., "10ms" from "5Mbps-goodDataRate10ms-goodDelay")
                delay_part = part.split("-goodDelay")[0]
                good_delay = delay_part.split("-goodDataRate")[1]
            except (ValueError, IndexError):
                pass
        elif "badDataRate" in part and "badDelay" in part:
            # Extract badDataRate and badDelay from directory name
            # Format: XMbps-badDataRateYms-badDelay
            try:
                # Extract badDataRate (e.g., "5Mbps" from "5Mbps-badDataRate10ms-badDelay")
                bad_data_rate = part.split("-badDataRate")[0]
                # Extract badDelay (e.g., "10ms" from "5Mbps-badDataRate10ms-badDelay")
                delay_part = part.split("-badDelay")[0]
                bad_delay = delay_part.split("-badDataRate")[1]
            except (ValueError, IndexError):
                pass

    return num_paths, num_bad_paths, simulation_time, good_data_rate, good_delay, bad_data_rate, bad_delay


def build_plot_title(plot_data: PlotData) -> str:
    """Build plot title."""
    base_title = plot_data.metric_name if not plot_data.header else plot_data.metric_name
    return f"{base_title}\ngoodDataRate={plot_data.good_data_rate}, goodDelay={plot_data.good_delay}, badDataRate={plot_data.bad_data_rate}, badDelay={plot_data.bad_delay}\nnumPaths={plot_data.num_paths}, numBadPaths={plot_data.num_bad_paths}"


def get_metric_group(metric_name: str) -> Optional[str]:
    """Get the group name for a metric."""
    for group_name, metrics in METRIC_GROUPS.items():
        if metric_name in metrics:
            return group_name
    return None


def prepare_plot_data(data_file: Path) -> Optional[PlotData]:
    """Prepare plot data from a data file."""
    metric_name = data_file.stem
    if metric_name not in SUPPORTED_METRICS:
        return None

    parsed = read_trace_file(data_file, metric_name)
    if parsed is None:
        print(f"Skip empty/unreadable: {data_file}")
        return None

    time_values, metric_values, contexts, header = parsed

    # Filter out invalid values
    mask = np.isfinite(time_values) & np.isfinite(metric_values)
    if metric_name not in ("throughput", "goodput", "btlbw", "btlbw_raw"):
        mask &= metric_values != 0

    time_values = time_values[mask]
    metric_values = metric_values[mask]
    if contexts is not None:
        contexts = contexts[mask]

    if len(time_values) == 0:
        print(f"Skip without valid points: {data_file}")
        return None

    y_label = METRIC_CONFIG[metric_name]["label"]
    positive_values = metric_values[metric_values > 0]

    num_paths, num_bad_paths, simulation_time, good_data_rate, good_delay, bad_data_rate, bad_delay = extract_plot_metadata(data_file)

    return PlotData(
        data_file=data_file,
        output_file=build_output_path(data_file, simulation_time),
        metric_name=metric_name,
        num_paths=num_paths,
        num_bad_paths=num_bad_paths,
        simulation_time=simulation_time,
        good_data_rate=good_data_rate,
        good_delay=good_delay,
        bad_data_rate=bad_data_rate,
        bad_delay=bad_delay,
        time_values=time_values,
        plot_values=metric_values,
        contexts=contexts,
        header=header,
        y_label=y_label,
        y_min_positive=float(np.min(positive_values)),
        y_max=float(np.max(metric_values)),
    )


def configure_axis_ticks(axis, simulation_time: str):
    """Configure axis ticks."""
    sim_time = int(simulation_time)
    axis.set_xlim(1, sim_time)
    axis.xaxis.set_major_locator(FixedLocator(np.arange(1, sim_time + 1, 1)))
    axis.xaxis.set_minor_locator(AutoMinorLocator(2))
    axis.yaxis.set_major_locator(plt.MaxNLocator(16))
    axis.yaxis.set_minor_locator(AutoMinorLocator(4))
    axis.tick_params(axis="both", which="major", labelsize=10)
    axis.tick_params(axis="both", which="minor", length=3)


def plot_data_on_axis(axis, plot_data: PlotData, show_xlabel: bool = False):
    """Plot data on a given axis."""
    if plot_data.contexts is None:
        axis.plot(plot_data.time_values, plot_data.plot_values, linewidth=1.2)
    else:
        unique_contexts = np.unique(plot_data.contexts)
        show_legend = len(unique_contexts) <= 8

        for context in unique_contexts:
            context_mask = plot_data.contexts == context
            label = context
            if len(label) > 60:
                label = "..." + label[-57:]
            axis.plot(
                plot_data.time_values[context_mask],
                plot_data.plot_values[context_mask],
                linewidth=1.2,
                label=label,
            )

        if show_legend:
            axis.legend(loc="best", fontsize=9)

    if show_xlabel:
        axis.set_xlabel("Time (s)")
    axis.set_ylabel(plot_data.y_label)


def should_render(plot_data: PlotData, metric_y_min_positive: float, metric_y_max: float) -> bool:
    """Check if plot needs to be rendered."""
    if not plot_data.output_file.exists():
        return True

    output_mtime = plot_data.output_file.stat().st_mtime
    if plot_data.data_file.stat().st_mtime > output_mtime:
        return True

    return plot_data.y_max < metric_y_max or plot_data.y_min_positive > metric_y_min_positive


def render_plot(plot_data: PlotData, metric_y_min_positive: float, metric_y_max: float):
    """Render plot for a single metric."""
    fig, axis = plt.subplots(figsize=(12, 8))

    plot_data_on_axis(axis, plot_data, show_xlabel=True)
    # axis.set_title(build_plot_title(plot_data))
    axis.set_ylim(bottom=0, top=metric_y_max * 1.05 if metric_y_max > 0 else 1.0)
    configure_axis_ticks(axis, plot_data.simulation_time)
    axis.grid(True, which="major", linewidth=0.8)
    axis.grid(True, which="minor", linewidth=0.4, alpha=0.5)

    plot_data.output_file.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(plot_data.output_file, dpi=300)
    plt.close(fig)


def iter_data_files(filter_pattern: Optional[str] = None):
    """Iterate over all data files, optionally filtering by pattern."""
    for data_file in sorted(DATA_DIR.rglob("*.data")):
        if data_file.stem not in SUPPORTED_METRICS:
            continue

        # Apply filter if provided
        if filter_pattern:
            file_path_str = str(data_file)
            try:
                # Try as regex first
                if not re.search(filter_pattern, file_path_str):
                    continue
            except re.error:
                # If not a valid regex, treat as simple substring
                if filter_pattern not in file_path_str:
                    continue

        yield data_file


def calculate_metric_ranges(prepared_plots: list[PlotData]) -> dict[tuple[str, str, str, int], tuple[float, float]]:
    """Calculate y-axis ranges for each metric group and goodDataRate/goodDelay/numPaths combination."""
    metric_ranges = {}

    # Group plots by metric group and goodDataRate/goodDelay/numPaths combination
    group_plots = {}
    for plot_data in prepared_plots:
        group = get_metric_group(plot_data.metric_name)
        if group:
            # Create a composite key: (metric_name, good_data_rate, good_delay, num_paths)
            key = (plot_data.metric_name, plot_data.good_data_rate, plot_data.good_delay, plot_data.num_paths)
            if key not in group_plots:
                group_plots[key] = []
            group_plots[key].append(plot_data)

    # Calculate ranges for each combination
    for key, plots in group_plots.items():
        combined_min = min(plot_data.y_min_positive for plot_data in plots)
        combined_max = max(plot_data.y_max for plot_data in plots)

        # Assign the same range to all plots with the same metric and goodDataRate/goodDelay/numPaths
        metric_ranges[key] = (combined_min, combined_max)

    return metric_ranges


def main():
    """Main function."""
    parser = argparse.ArgumentParser(description="Generate plots from trace data files")
    parser.add_argument(
        "filter",
        nargs="?",
        default=None,
        help="Filter files by string or regular expression pattern (matches against file path)"
    )
    args = parser.parse_args()

    files = list(iter_data_files(args.filter))
    if not files:
        if args.filter:
            print(f"No supported .data files found in {DATA_DIR} matching pattern: {args.filter}")
        else:
            print(f"No supported .data files found in {DATA_DIR}")
        return

    total_files = len(files)
    print(f"Found {total_files} data files to process")

    # Prepare plots with progress tracking
    prepared_plots = []
    for i, data_file in enumerate(files, 1):
        try:
            plot_data = prepare_plot_data(data_file)
        except Exception as e:
            print(f"\nError processing file {data_file}: {e}")
            continue
        if plot_data is not None:
            prepared_plots.append(plot_data)
        progress = (i / total_files) * 100
        print(f"\rReading files: {progress:.1f}% ({i}/{total_files})", end="", flush=True)

    print()  # New line after reading progress

    if not prepared_plots:
        print("No valid plot data found")
        return

    # Calculate metric ranges based on groups and goodDataRate/goodDelay combinations
    metric_ranges = calculate_metric_ranges(prepared_plots)

    # Render plots with progress tracking
    total_plots = len(prepared_plots)
    for i, plot_data in enumerate(prepared_plots, 1):
        key = (plot_data.metric_name, plot_data.good_data_rate, plot_data.good_delay, plot_data.num_paths)
        metric_y_min_positive, metric_y_max = metric_ranges.get(key, (0.0, 1.0))
        render_plot(plot_data, metric_y_min_positive, metric_y_max)
        progress = (i / total_plots) * 100
        print(f"\rRendering plots: {progress:.1f}% ({i}/{total_plots})", end="", flush=True)

    print()  # New line after rendering progress
    print(f"Completed: {total_plots} plots rendered")


if __name__ == "__main__":
    main()
