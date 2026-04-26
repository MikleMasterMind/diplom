#!/usr/bin/env python3

"""Batch runner for ns-3 experiments.

Python equivalent of [`run-experiments.sh`](run-experiments.sh).
"""

from __future__ import annotations

import subprocess
import sys
from typing import Iterable

EXPERIMENT_NAME = "my-experiment"
SIMULATION_TIME = 60

# Each configuration uses the format:
# (good_data_rate, good_delay, bad_data_rate, bad_delay, realCongestion)
NETWORK_CONFIGS = [
    # {
        # ("5Mbps", "1ms"): [
        #      ("5Mbps", "10ms"),
            #  ("5Mbps", "5ms"),
        # ]
    # },
    # {
    #    ("5Mbps", "10ms"): [
    #        ("5Mbps", "50ms"),
    #        ("5Mbps", "100ms"),
    #    ]
    # },
    # {
    #     ("50Mbps", "1ms"): [
    #         ("50Mbps", "5ms"),
    #         ("50Mbps", "10ms"),
    #     ]
    # },
    # {
    #     ("5Mbps", "10ms"): [
    #         ("5Mbps", "20ms"),
    #         ("5Mbps", "50ms"),
    #         ("5Mbps", "100ms"),
    #         ("2Mbps", "10ms"),
    #         ("1Mbps", "10ms"),
    #     ]
    # },
    {
        ("50Mbps", "10ms"): [
            ("50Mbps", "100ms", "")
            # ("5Mbps", "10ms", "", ""),
            # ("5Mbps", "10ms", "", ""),
            # ("5Mbps", "10ms", "", ""),
            # ("5Mbps", "10ms", "", ""),
        ]
    },
]

# Each configuration uses the format:
# (num_paths, num_bad_paths)
PATH_CONFIGS = [
    # ("1", "0"),
    ("2", "0"),
    ("2", "1"),
    # ("3", "0"),
    # ("3", "1"),
]

SACK_CONFIGS = [
    # True,
    False,
]


def format_bool(value: bool) -> str:
    return "true" if value else "false"


def run_experiment(
    good_data_rate: str,
    good_delay: str,
    bad_data_rate: str,
    bad_delay: str,
    realCongestion: str,
    num_paths: str,
    num_bad_paths: str,
    sim_time: int,
    sack_enabled: bool,
) -> bool:
    # print("========================================")
    # print("Running experiment with parameters:")
    # print(f"  Good Data Rate: {good_data_rate}")
    # print(f"  Good Delay: {good_delay}")
    # print(f"  Bad Data Rate: {bad_data_rate}")
    # print(f"  Bad Delay: {bad_delay}")
    # print(f"  Real Congestion enabled: {realCongestion}")
    # print(f"  Number of Paths: {num_paths}")
    # print(f"  Number of Bad Paths: {num_bad_paths}")
    # print(f"  Simulation Time: {sim_time} seconds")
    # print(f"  SACK Enabled: {format_bool(sack_enabled)}")
    # print("========================================")

    # Build command arguments
    args = [
        f"--goodDataRate={good_data_rate}",
        f"--goodDelay={good_delay}",
        f"--badDataRate={bad_data_rate} "
        f"--badDelay={bad_delay} "
        f"--numPaths={num_paths}",
        f"--numBadPaths={num_bad_paths}",
        f"--simulationTime={sim_time}",
        f"--sackEnabled={format_bool(sack_enabled)}",
    ]
    
    if realCongestion:
        args.append("--realCongestion=true")
    else:
        args.append("--realCongestion=false")
    
    command = [
        "./ns3",
        "run",
        f"{EXPERIMENT_NAME} " + " ".join(args),
    ]

    print(f"Running command: {' '.join(command)}")
    print()

    result = subprocess.run(command, check=False)
    if result.returncode == 0:
        print("✓ Experiment completed successfully")
        print()
        return True

    print("✗ Experiment failed")
    print()
    return False


def iter_experiments() -> Iterable[tuple[str, str, str, str, str, str, str, bool]]:
    for network_config in NETWORK_CONFIGS:
        for (good_data_rate, good_delay), bad_configs in network_config.items():
            for num_paths, num_bad_paths in PATH_CONFIGS:
                if num_bad_paths == "0":
                    for bad_data_rate, bad_delay, realCongestion  in [bad_configs[0]]:
                        for sack_enabled in SACK_CONFIGS:
                            yield (
                                good_data_rate,
                                good_delay,
                                bad_data_rate,
                                bad_delay,
                                realCongestion,
                                num_paths,
                                num_bad_paths,
                                sack_enabled,
                            )
                else:
                    # Run all configurations for bad paths
                    for bad_data_rate, bad_delay, realCongestion in bad_configs:
                        for sack_enabled in SACK_CONFIGS:
                            yield (
                                good_data_rate,
                                good_delay,
                                bad_data_rate,
                                bad_delay,
                                realCongestion,
                                num_paths,
                                num_bad_paths,
                                sack_enabled,
                            )


def main() -> int:
    total_experiments = sum(
        (
            1 if num_bad_paths == "0" else len(bad_configs)
        ) * len(SACK_CONFIGS)
        for network_config in NETWORK_CONFIGS
        for bad_configs in network_config.values()
        for _, num_bad_paths in PATH_CONFIGS
    )

    # print("Starting batch experiment execution...")
    # print(f"Network configurations: {len(NETWORK_CONFIGS)}")
    # print(f"Path configurations: {len(PATH_CONFIGS)}")
    # print(f"SACK configurations: {len(SACK_CONFIGS)}")
    print(f"Total experiments: {total_experiments}")
    print()

    success_count = 0
    failed_count = 0
    completed_count = 0

    for (
        good_data_rate,
        good_delay,
        bad_data_rate,
        bad_delay,
        realCongestion,
        num_paths,
        num_bad_paths,
        sack_enabled,
    ) in iter_experiments():
        if run_experiment(
            good_data_rate,
            good_delay,
            bad_data_rate,
            bad_delay,
            realCongestion,
            num_paths,
            num_bad_paths,
            SIMULATION_TIME,
            sack_enabled,
        ):
            success_count += 1
        else:
            failed_count += 1

        completed_count += 1
        progress_percent = (completed_count / total_experiments) * 100
        print(
            f"Progress: {completed_count}/{total_experiments} "
            f"({progress_percent:.1f}%)"
        )
        print()

    print("========================================")
    print("Batch experiment execution completed")
    print(f"Total experiments: {success_count + failed_count}")
    print(f"Successful: {success_count}")
    print(f"Failed: {failed_count}")
    print("========================================")

    return 0 if failed_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
