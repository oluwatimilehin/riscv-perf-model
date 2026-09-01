import json

from collections import defaultdict
from pathlib import Path
from typing import Dict, List

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

plots_dir = "plots"


def plot_confidence_error_correlation(results: Dict[int, Dict[str, List[float]]]):
    records = []
    for sample_size, metrics in results.items():
        coefficient_of_variation_pcts = metrics["coefficient_of_variation_pcts"]
        half_width_95_pcts = metrics["half_width_95_pcts"]
        error_pcts = metrics["error_pcts"]

        for cv_val, hw_val, err_val in zip(
            coefficient_of_variation_pcts, half_width_95_pcts, error_pcts
        ):
            records.append(
                {
                    "sample_size": sample_size,
                    "coefficient_of_variation_pct": cv_val,
                    "half_width_95_pct": hw_val,
                    "error_pct": err_val,
                }
            )

    df = pd.DataFrame(records)

    plt.figure(figsize=(7, 5))
    sns.scatterplot(
        data=df,
        x="coefficient_of_variation_pct",
        y="error_pct",
        hue="sample_size",
        s=80,
    )
    plt.title("Error % vs Coefficient of Variation %")
    plt.xlabel("Coefficient of Variation (%)")
    plt.ylabel("Error (%)")
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.savefig(f"{plots_dir}/error_vs_cv.png", dpi=300, bbox_inches="tight")
    plt.close()

    plt.figure(figsize=(7, 5))
    sns.scatterplot(
        data=df, x="half_width_95_pct", y="error_pct", hue="sample_size", s=80
    )
    plt.title("Error % vs Half-Width 95% CI")
    plt.xlabel("Half-Width 95% (%)")
    plt.ylabel("Error (%)")
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.savefig(f"{plots_dir}/error_vs_hw95.png", dpi=300, bbox_inches="tight")
    plt.close()


def analyse_results():
    base_results_dir = Path("results/")
    convergence_criteria_to_results_dir = {
        "current_ipc": Path(f"{base_results_dir}/current_ipc"),
        "expected_ipc": Path(f"{base_results_dir}/expected_ipc"),
    }

    execution_time_per_num_instructions: Dict[str, Dict[str, List[float]]] = (
        defaultdict(lambda: defaultdict(list))
    )

    deterministic_results_dir = Path(f"{base_results_dir}/deterministic")
    for deterministic_result in deterministic_results_dir.rglob("*"):
        with open(deterministic_result) as f:
            result_json = json.load(f)["results"]
            execution_time_per_num_instructions["deterministic"][
                "num_instructions"
            ].append(result_json["retired_insts"]["mean"])
            execution_time_per_num_instructions["deterministic"][
                "execution_time"
            ].append(result_json["execution_time"]["mean"])

    confidence_error_correlation_per_sample_size: Dict[int, Dict[str, List[float]]] = (
        defaultdict(lambda: defaultdict(list))
    )

    error_per_convergence_criteria: Dict[str, Dict[int, List[float]]] = defaultdict(
        lambda: defaultdict(list)
    )

    for (
        convergence_criteria,
        results_dir,
    ) in convergence_criteria_to_results_dir.items():
        result_files = [file for file in results_dir.rglob("*")]

        for result_file in result_files:
            with open(result_file) as f:
                result_json = json.load(f)
                num_runs = result_json["num_runs"]

                result_json = result_json["results"]
                ipc = result_json["ipc"]

                first_confidence_interval = ipc["confidence_intervals"][0]
                assert (
                    first_confidence_interval["level"] == 0.95
                ), "First confidence interval was not 95%. Do this properly with a dict!"

                confidence_error_correlation_per_sample_size[num_runs][
                    "coefficient_of_variation_pcts"
                ].append(ipc["coefficient_of_variation_pct"])
                confidence_error_correlation_per_sample_size[num_runs][
                    "half_width_95_pcts"
                ].append(first_confidence_interval["half_width_pct"])
                confidence_error_correlation_per_sample_size[num_runs][
                    "error_pcts"
                ].append(abs(ipc["error_pct"]))

                execution_time_per_num_instructions[convergence_criteria][
                    "num_instructions"
                ].append(result_json["retired_insts"]["actual"])
                execution_time_per_num_instructions[convergence_criteria][
                    "execution_time"
                ].append(result_json["execution_time"]["mean"])

                error_per_convergence_criteria[convergence_criteria][num_runs].append(
                    abs(ipc["error_pct"])
                )

    print(json.dumps(confidence_error_correlation_per_sample_size, indent=4))
    print(json.dumps(execution_time_per_num_instructions, indent=4))
    print(json.dumps(error_per_convergence_criteria, indent=4))
    plot_confidence_error_correlation(confidence_error_correlation_per_sample_size)


if __name__ == "__main__":
    print(analyse_results())
