import argparse

import base64
import hashlib

import json
import re
import statistics
import subprocess
import time
import traceback


from typing import Dict, List
from pathlib import Path
from statistics import NormalDist

import datetime

SCRIPT_DIR = Path(__file__).resolve().parent
PARENT_DIR = SCRIPT_DIR.parent
EXECUTABLE_DIR = f"{PARENT_DIR}/fastdebug"


def olympia_exec(
    trace_path: Path,
    config_path: Path,
    probabilistic_mode: bool,
    convergence_metric: str,
    convergence_threshold: float,
    convergence_check_starting_interval: int,
) -> Dict[str, object]:
    trace_name = trace_path.stem
    report_prefix = f"{SCRIPT_DIR}/reports/{trace_name}"

    if probabilistic_mode:
        report_prefix += f"-probabilistic-{convergence_metric}"

    report_path = f"{report_prefix}.json"
    args = [
        f"./olympia",
        "--config-file",
        config_path.absolute(),
        trace_path,
        "--auto-summary",
        "off",
        "--report",
        "top.cpu.core0.rob",
        "reports/core_stats.yaml",
        report_path,
        "json",
    ]

    if probabilistic_mode:
        args.extend(
            [
                "--probabilistic-mode",
                "--convergence-metric",
                convergence_metric,
                "--convergence-threshold",
                str(convergence_threshold),
                "--convergence-check-interval",
                str(convergence_check_starting_interval),
            ]
        )

    print(
        f"Running simulation for {trace_path} using config: {config_path}. Probabilistic mode = {probabilistic_mode} with command {' '.join(str(arg) for arg in args)}"
    )

    start_time = time.time()
    olympia_process = subprocess.run(
        args, capture_output=True, text=True, cwd=EXECUTABLE_DIR
    )
    end_time = time.time()

    if olympia_process.returncode != 0:
        print(f"Simulation failed with error: {olympia_process.stderr.strip()}")
        return {}

    output = olympia_process.stdout
    match = re.search(r"wall\((\d+\.\d+)\)", output)

    wall_time = end_time - start_time
    print(f"Output: {output}")
    if not match:
        print(
            f"Simulation ran too quickly to determine the wall time. Using the measured python time as wall time"
        )
    else:
        wall_time = float(match.group(1))

    with open(report_path) as f:
        results = json.load(f)["Statistics"]["rob"]
        results["wall_time_s"] = wall_time

    return results


def confidence_interval(data, confidence=0.95):
    dist = NormalDist.from_samples(data)
    z = NormalDist().inv_cdf((1 + confidence) / 2.0)
    h = dist.stdev * z / ((len(data) - 1) ** 0.5)
    return dist.mean - h, dist.mean + h


confidence_levels = [0.95, 0.97, 0.99]


def calculate_confidence_levels(data) -> List[Dict[str, float]]:
    results: List[Dict[str, float]] = []

    for level in confidence_levels:
        lower, upper = confidence_interval(data, level)
        results.append({"level": level, "lower": lower, "upper": upper})

    return results


def run_trace(
    num_runs: int,
    trace_path: Path,
    config_path: Path,
    probabilistic_mode: bool = False,
    convergence_metric: str = "current_ipc",
    convergence_threshold: float = 0.00005,
    convergence_check_interval: int = 10000,
) -> Dict[str, object]:

    results: Dict[str, object] = {}

    ipcs = []
    execution_times = []
    all_retired_insts = []

    for i in range(num_runs):
        print(f"Run {i + 1} for trace: {trace}")
        single_res: Dict[str, object] = olympia_exec(
            trace_path,
            config_path,
            probabilistic_mode,
            convergence_metric,
            convergence_threshold,
            convergence_check_interval,
        )

        if not single_res:
            break

        wall_times_s = single_res["wall_time_s"]
        ipc = single_res["ipc"]["val"]
        retired_inst = single_res["total_number_retired"]["val"]

        execution_times.append(wall_times_s)
        if i == 0 and not probabilistic_mode:
            # Do this just once as it'll be deterministic
            ipcs.append(ipc)
            all_retired_insts.append(retired_inst)

        if probabilistic_mode:
            all_retired_insts.append(retired_inst)
            if convergence_metric == "current_ipc":
                ipcs.append(ipc)
                continue

            ipcs.append(
                single_res["stationary_dist_ipc"]["val"]
            )  # Use the stationary distribution IPC if we're using the expected_ipc metric

    mean = statistics.mean(ipcs)
    ipc = {"mean": mean}
    execution_time = {
        "mean": statistics.mean(execution_times),
        "stddev": statistics.stdev(execution_times),
    }
    retired_insts = {
        "mean": statistics.mean(all_retired_insts),
    }

    results["ipc"] = ipc
    results["execution_time"] = execution_time
    results["retired_insts"] = retired_insts

    if probabilistic_mode:
        results["retired_insts"]["stddev"] = statistics.stdev(all_retired_insts)

        ipc_stddev = statistics.stdev(ipcs)
        results["ipc"]["stddev"] = ipc_stddev
        results["ipc"]["confidence_intervals"] = calculate_confidence_levels(ipcs)
        results["ipc"]["coefficient_of_variation_pct"] = (ipc_stddev / mean) * 100
    return results


def hash_simulation_config(
    num_runs: int, convergence_threshold: float, convergence_check_interval: int
) -> str:
    concat_config = f"{num_runs}-{convergence_threshold}-{convergence_check_interval}"
    hasher = hashlib.sha1(concat_config.encode("utf-8"))
    return base64.urlsafe_b64encode(hasher.digest()[:6]).decode("utf-8")


if __name__ == "__main__":
    traces_dir = Path(f"{EXECUTABLE_DIR}/traces")
    trace_extensions = {".zstf", ".stf"}

    configs_dir = Path(f"{EXECUTABLE_DIR}/arches")
    config_extensions = {".yaml", ".yml"}

    parser = argparse.ArgumentParser(
        prog="Validation script for probabilistic simulation"
    )

    parser.add_argument(
        "-n",
        "--num-runs",
        type=int,
        default=10,
        help="How many times to run the simulation",
    )

    parser.add_argument(
        "-t",
        "--traces",
        nargs="+",
        default=[],
        help="Provide the path to one or more trace files in zstf or stf format. Will default to the traces in traces_dir directory if not provided.",
    )

    parser.add_argument(
        "--traces-dir",
        type=str,
        default=traces_dir,
        help="Path to a directory containing trace files. ",
    )

    parser.add_argument(
        "-c",
        "--configs",
        nargs="+",
        default=[],
        help="Provide the path to one or more YAML files specifying the core's configuration. Will default to the configs in the configs_dir directory if not provided.",
    )

    parser.add_argument(
        "--configs-dir",
        type=str,
        default=configs_dir,
        help="Path to a directory containing config files.",
    )

    parser.add_argument(
        "--probabilistic-mode",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Whether to run simulation in probabilistic or deterministic mode",
    )

    parser.add_argument(
        "--convergence-metric",
        type=str,
        default="current_ipc",
        help="Whether to converge using the 'current_ipc' or the 'expected_ipc'",
    )

    parser.add_argument(
        "--convergence-threshold",
        type=float,
        default=0.0005,
        help="Maximum difference between consecutive values (defined by the interval) of convergence metric before terminating",
    )

    parser.add_argument(
        "--convergence-check-interval",
        type=int,
        default=10000,
        help="How many cycles between consecutive convergence checks",
    )

    args = parser.parse_args()

    traces = [Path(trace) for trace in args.traces]
    if not traces:
        traces = [
            file
            for file in traces_dir.rglob("*")
            if file.is_file() and file.suffix.lower() in trace_extensions
        ]

    configs = [Path(config) for config in args.configs]
    if not configs:
        configs = [
            file
            for file in configs_dir.rglob("*")
            if file.is_file() and file.suffix.lower() in config_extensions
        ]

    probabilistic_mode = args.probabilistic_mode
    num_runs = args.num_runs
    convergence_metric = args.convergence_metric
    convergence_threshold = args.convergence_threshold
    convergence_check_interval = args.convergence_check_interval

    start_time = time.time()
    trace_results: Dict[str, object] = {"num_runs": num_runs}

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    deterministic_results_dir = "results/deterministic"
    results_dir = (
        deterministic_results_dir
        if not probabilistic_mode
        else f"results/{convergence_metric}"
    )

    print(
        f"Processing traces: {traces} with {num_runs} runs for each config: {configs}"
    )
    for trace in traces:
        single_trace_res = {
            "trace": trace.name,
            "num_runs": num_runs,
            "probabilistic_mode": probabilistic_mode,
        }

        if probabilistic_mode:
            single_trace_res["convergence_threshold"] = convergence_threshold
            single_trace_res["convergence_check_interval"] = convergence_check_interval
            single_trace_res["convergence_metric"] = convergence_metric

        for config in configs:
            trace_start_time = time.time()
            single_trace_res["config"] = config.name
            print(
                f"Simulating {trace.name} with config: {config.name} and parameters: probabilistic_mode={probabilistic_mode}, convergence_metric={convergence_metric}, convergence_threshold={convergence_threshold}, convergence_check_interval={convergence_check_interval}"
            )

            try:
                deterministic_file_name_prefix = f"{trace.stem}_{config.stem}"

                result_file_name_prefix = (
                    deterministic_file_name_prefix
                    if not probabilistic_mode
                    else f"{deterministic_file_name_prefix}_{hash_simulation_config(num_runs, convergence_threshold, convergence_check_interval)}"
                )

                result_file_path = Path(f"{results_dir}/{result_file_name_prefix}.json")

                if result_file_path.is_file():
                    print(f"Already simulated {trace.name} with {config}")
                    continue

                results = run_trace(
                    num_runs,
                    trace,
                    config,
                    probabilistic_mode,
                    convergence_metric,
                    convergence_threshold,
                    convergence_check_interval,
                )

                if probabilistic_mode:
                    deterministic_file_path = Path(
                        f"{deterministic_results_dir}/{deterministic_file_name_prefix}.json"
                    )
                    print(f"Checking for {deterministic_file_path}")
                    if deterministic_file_path.is_file():
                        print(f"Found existing {deterministic_file_path}")
                        with open(deterministic_file_path) as f:
                            actual_ipc = json.load(f)["results"]["ipc"]["mean"]
                            computed_ipc = results["ipc"]["mean"]
                            results["ipc"]["error_pct"] = (
                                (actual_ipc - computed_ipc) / actual_ipc
                            ) * 100

                single_trace_res["results"] = results

                print(f"Result {trace.name}: {single_trace_res}")
                Path(results_dir).mkdir(parents=True, exist_ok=True)
                with open(result_file_path, "w", encoding="utf-8") as f:
                    f.write(json.dumps(single_trace_res, indent=4))

                trace_end_time = time.time()
                print(
                    f"Written results for {trace.name} to {result_file_path} in {trace_end_time - trace_start_time} seconds"
                )

            except Exception as e:
                print(
                    f"Failed to process {trace.name} with config: {config.name}.\n"
                    f"Error Details: {e}\n"
                    f"Traceback:\n{traceback.format_exc()}"
                )

    print(f"Completed simulation")
