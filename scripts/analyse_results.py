import json
import statistics

from collections import defaultdict
from pathlib import Path
from typing import Dict, List


import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
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


def plot_execution_time_against_num_instructions(
    results: Dict[str, Dict[str, List[float]]],
):
    records = []
    for mode, metrics in results.items():
        for insts, execution_time in zip(
            metrics["num_instructions"], metrics["execution_time"]
        ):
            records.append(
                {
                    "mode": mode,
                    "num_instructions": insts,
                    "execution_time": execution_time,
                }
            )

    df = pd.DataFrame(records)
    df_avg = df.groupby(["mode", "num_instructions"], as_index=False)[
        "execution_time"
    ].mean()

    plt.figure(figsize=(8, 5))
    sns.scatterplot(
        data=df_avg,
        x="num_instructions",
        y="execution_time",
        hue="mode",
        style="mode",
        s=70,
    )

    plt.gca().xaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, p: f"{x * 1e-6:.1f}M")
    )

    plt.title("Average Execution Time vs. Instructions by Mode", fontsize=12)
    plt.xlabel("Number of Instructions", fontsize=10)
    plt.ylabel("Average Execution Time (s)", fontsize=10)
    plt.grid(True, linestyle="--", alpha=0.5)

    plt.legend(title="Execution Mode", bbox_to_anchor=(1.02, 1), loc="upper left")

    plt.savefig(
        f"{plots_dir}/execution_time_vs_num_instructions.png",
        dpi=300,
        bbox_inches="tight",
    )
    plt.close()


def plot_error_per_convergence_criteria(results: Dict[str, Dict[int, List[float]]]):
    records = []

    for convergence_criteria, sample_dict in results.items():
        for sample_size, error_list in sample_dict.items():
            mean_error = statistics.mean(error_list)
            records.append(
                {
                    "convergence_criteria": convergence_criteria,
                    "sample_size": sample_size,
                    "mean_error": mean_error,
                }
            )

    df_means = pd.DataFrame(records)
    df_means = df_means.sort_values("sample_size")

    plt.figure(figsize=(8, 5))
    ax = sns.barplot(
        data=df_means,
        x="sample_size",
        y="mean_error",
        hue="convergence_criteria",
        palette="muted",
        width=0.8,
    )

    # Displays the value on top of each bar
    for p in ax.patches:
        if p.get_height() > 0:
            ax.annotate(
                f"{p.get_height():.2f}%",
                (p.get_x() + p.get_width() / 2.0, p.get_height()),
                ha="center",
                va="center",
                xytext=(0, 5),
                textcoords="offset points",
                fontsize=9,
            )

    plt.ylim(0, 50)
    plt.title(
        "Mean Error % by Sample Size and Convergence Criteria",
        fontsize=12,
        fontweight="bold",
    )
    plt.xlabel("Sample Size", fontsize=10)
    plt.ylabel("Mean Error (%)", fontsize=10)
    plt.grid(True, axis="y", linestyle="--", alpha=0.5)

    plt.legend(title="Convergence Criteria", bbox_to_anchor=(1.02, 1), loc="upper left")
    plt.tight_layout()
    plt.savefig(f"{plots_dir}/mean_error_comparison.png", dpi=300, bbox_inches="tight")


def plot_error_per_trace(trace_results: List[Dict[str, object]]):
    df = pd.DataFrame(trace_results)

    unique_configs = df["config"].unique()

    for config in unique_configs:
        df_config = df[df["config"] == config]

        unique_samples = sorted(df_config["num_runs"].unique())
        num_samples = len(unique_samples)

        fig, axes = plt.subplots(
            nrows=num_samples,
            ncols=1,
            figsize=(9, 4 * num_samples),
            sharex=True,
        )

        if num_samples == 1:
            axes = [axes]

        for ax, sample_size in zip(axes, unique_samples):
            df_sub = df_config[df_config["num_runs"] == sample_size]

            sns.barplot(
                data=df_sub,
                x="trace",
                y="error_pct",
                hue="convergence_metric",
                palette="muted",
                ax=ax,
            )

            ax.axhline(0, color="black", linewidth=0.8)
            ax.grid(True, axis="y", linestyle="--", alpha=0.5)

            for container in ax.containers:
                ax.bar_label(
                    container,
                    fmt="%.2f%%",
                    padding=3,
                    fontsize=9,
                    fontweight="bold",
                )
            ax.set_ylim(-10, 15)

            ax.set_title(
                f"Number of Runs: {sample_size}", fontsize=11, fontweight="bold"
            )
            ax.set_ylabel("Error (%)", fontsize=10)
            ax.legend(title="Convergence Metric", loc="upper right")

        axes[-1].set_xlabel("Trace", fontsize=11, fontweight="bold")
        plt.xticks(rotation=20)

        plt.suptitle(
            f"Percentage Error Per Convergence Metric for Config: {config}",
            fontsize=14,
            fontweight="bold",
            y=1.01,
        )
        plt.tight_layout()

        output_filename = f"{plots_dir}/error_analysis_{config}.png"
        plt.savefig(output_filename, dpi=300, bbox_inches="tight")
        plt.close()
        print(f"Saved plot: {output_filename}")


def analyse_results():
    base_results_dir = Path("results/")
    convergence_metric_to_results_dir = {
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

    error_per_convergence_metric: Dict[str, Dict[int, List[float]]] = defaultdict(
        lambda: defaultdict(list)
    )

    trace_results: List[Dict[str, object]] = []

    for (
        convergence_metric,
        results_dir,
    ) in convergence_metric_to_results_dir.items():
        result_files = [file for file in results_dir.rglob("*")]

        for result_file in result_files:
            with open(result_file) as f:
                result_json = json.load(f)
                trace = Path(result_json["trace"]).stem
                num_runs = result_json["num_runs"]
                config = Path(result_json["config"]).stem
                result_json = result_json["results"]
                ipc = result_json["ipc"]
                ipc_error = ipc["error_pct"]

                trace_results.append(
                    {
                        "num_runs": num_runs,
                        "config": config,
                        "trace": trace,
                        "convergence_metric": convergence_metric,
                        "error_pct": ipc_error,
                    }
                )

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
                ].append(abs(ipc_error))

                if num_runs == 10:  # Parallel execution slows things down
                    execution_time_per_num_instructions[convergence_metric][
                        "num_instructions"
                    ].append(result_json["retired_insts"]["actual"])
                    execution_time_per_num_instructions[convergence_metric][
                        "execution_time"
                    ].append(result_json["execution_time"]["mean"])

                error_per_convergence_metric[convergence_metric][num_runs].append(
                    abs(ipc["error_pct"])
                )

    # print(json.dumps(confidence_error_correlation_per_sample_size, indent=4))
    # print(json.dumps(execution_time_per_num_instructions, indent=4))
    # print(json.dumps(error_per_convergence_metric, indent=4))
    # print(json.dumps(trace_results, indent=4))
    plot_error_per_trace(trace_results)
    plot_confidence_error_correlation(confidence_error_correlation_per_sample_size)
    plot_execution_time_against_num_instructions(execution_time_per_num_instructions)
    plot_error_per_convergence_criteria(error_per_convergence_metric)


if __name__ == "__main__":
    print(analyse_results())
