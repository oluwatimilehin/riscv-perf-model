import json
import re
import statistics
import subprocess
import time

from collections import defaultdict
from typing import Dict, List
from pathlib import Path

import datetime

SCRIPT_DIR = Path(__file__).resolve().parent
PARENT_DIR = SCRIPT_DIR.parent
EXECUTABLE_DIR = f"{PARENT_DIR}/fastdebug"


def olympia_exec(
    trace_path: Path,
    config_path: Path,
    probabilistic_mode: bool,
    convergence_threshold: float,
    convergence_check_starting_interval: int,
) -> Dict[str, object]:
    trace_name = trace_path.stem
    report_prefix = f"{SCRIPT_DIR}/reports/{trace_name}"

    if probabilistic_mode:
        report_prefix += "-probabilistic"

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


def run_trace(
    num_runs: int,
    trace_path: Path,
    config_path: Path,
    probabilistic_mode: bool = False,
    convergence_threshold: float = 0.00005,
    convergence_check_starting_interval: int = 10000,
) -> Dict[str, object]:

    results: Dict[str, object] = {"probabilistic_mode": probabilistic_mode}

    if probabilistic_mode:
        results["convergence_threshold"] = convergence_threshold
        results["convergence_check_starting_interval"] = (
            convergence_check_starting_interval
        )

    ipcs = []
    stationary_distribution_ipcs = []
    execution_times = []
    all_retired_insts = []

    for i in range(num_runs):
        single_res: Dict[str, object] = olympia_exec(
            trace_path,
            config_path,
            probabilistic_mode,
            convergence_threshold,
            convergence_check_starting_interval,
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
            ipcs.append(ipc)
            all_retired_insts.append(retired_inst)
            stationary_distribution_ipcs.append(
                single_res["stationary_dist_ipc"]["val"]
            )

    ipc = {"values": ipcs, "mean": statistics.mean(ipcs)}
    execution_time = {
        "values": execution_times,
        "mean": statistics.mean(execution_times),
    }
    retired_insts = {
        "values": all_retired_insts,
        "mean": statistics.mean(all_retired_insts),
    }

    results["ipc"] = ipc
    results["execution_time"] = execution_time
    results["retired_insts"] = retired_insts

    if probabilistic_mode:
        results["ipc"]["stddev"] = statistics.stdev(ipcs)
        stationary_distribution_ipc_stats = {}
        stationary_distribution_ipc_stats["values"] = stationary_distribution_ipcs
        stationary_distribution_ipc_stats["mean"] = statistics.mean(
            stationary_distribution_ipcs
        )
        stationary_distribution_ipc_stats["stddev"] = statistics.stdev(
            stationary_distribution_ipcs
        )

        results["stationary_distribution_ipc"] = stationary_distribution_ipc_stats

    return results


if __name__ == "__main__":
    traces_dir = Path(f"{EXECUTABLE_DIR}/traces")

    trace_extensions = {".zstf", ".stf"}
    traces = [
        file
        for file in traces_dir.rglob("*")
        if file.is_file() and file.suffix.lower() in trace_extensions
    ]

    # traces = [Path(f"{traces_dir}/core_riscv.zstf")]
    configs = [
        Path(f"{EXECUTABLE_DIR}/arches/small_core.yaml"),
        Path(f"{EXECUTABLE_DIR}/arches/medium_core.yaml"),
    ]
    trace_results: Dict[str, object] = {}

    num_runs = 3
    convergence_threshold = 0.0005
    convergence_check_starting_interval = 10000

    start_time = time.time()

    for trace in traces:
        trace_start_time = time.time()
        print(f"Processing {trace.name}")
        single_trace_res = {
            "trace": trace.name,
            "num_runs": num_runs,
        }

        all_configs_results = []
        for config in configs:
            config_res: Dict[str, object] = {"config": config.name}
            try:
                deterministic_results = run_trace(num_runs, trace, config, False)

                probabilistic_results = run_trace(
                    num_runs,
                    trace,
                    config,
                    True,
                    convergence_threshold,
                    convergence_check_starting_interval,
                )

                actual_ipc = deterministic_results["ipc"]["mean"]
                stationary_distribution_ipc = probabilistic_results[
                    "stationary_distribution_ipc"
                ]["mean"]
                probabilistic_computed_ipc = probabilistic_results["ipc"]["mean"]

                probabilistic_results["stationary_distribution_ipc"]["errorPct"] = (
                    (actual_ipc - stationary_distribution_ipc) / actual_ipc
                ) * 100
                probabilistic_results["ipc"]["errorPct"] = (
                    (actual_ipc - probabilistic_computed_ipc) / actual_ipc
                ) * 100
                probabilistic_results["true_ipc"] = actual_ipc

                config_res["deterministic_results"] = deterministic_results
                config_res["probabilistic_results"] = probabilistic_results

                all_configs_results.append(config_res)

                print(
                    f"Current results for {trace.name}: {json.dumps(trace_results[trace.stem])}"
                )
            except Exception as e:
                print(
                    f"Failed to process {trace.name} with config: {config.name}. Error: {e}"
                )

        single_trace_res["per_config_results"] = all_configs_results
        trace_end_time = time.time()
        print(
            f"Finished processing {trace.name} in {trace_end_time - trace_start_time} seconds"
        )

        trace_results[trace.stem] = single_trace_res

    end_time = time.time()

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    results_dir = "results"
    Path(results_dir).mkdir(parents=True, exist_ok=True)

    results_file = f"{results_dir}/results_{timestamp}.json"

    with open(results_file, "w", encoding="utf-8") as f:
        f.write(json.dumps(trace_results, indent=4))

    print(
        f"Results saved to {results_file} for {len(traces)} traces in {end_time - start_time} seconds"
    )
