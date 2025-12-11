import subprocess
import json
import os
import pandas
import matplotlib.pyplot as plt

def run_make():
    result = subprocess.run(['cd .. && make'], shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print("Make failed with the following output:")
        print(result.stdout)
        print(result.stderr)
        raise Exception("Make command failed")
    print("Compiled implementations...")

def run_single_opt_all(impl: str, texts: list) -> None:
    print(f"Running all texts for implementation: {impl}")
    os.makedirs(f'eval_results/{impl}', exist_ok=True)
    for input_text in texts:
        print(f"  Input text: {input_text}")
        # ./rANS_implementation texts/input.txt tmp_output --eval
        # change directory to parent directory and run command
        cmd = f'cd .. && ./{impl} texts/{input_text} tmp_output --eval'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"    Execution failed for {impl} with input {input_text}")
            print(result.stdout)
            print(result.stderr)
            raise Exception(f"Execution failed for {impl} with input {input_text}")
        print(f"    Completed {impl} with input {input_text}")
        # move outputs to eval_results/{impl}/
        text_name = input_text.split('.')[0]
        os.makedirs(f'eval_results/{impl}/{text_name}', exist_ok=True)
        subprocess.run(f'mv ../tmp_output.decoded eval_results/{impl}/{text_name}/{input_text}.decoded', shell=True)
        subprocess.run(f'mv ../tmp_output.out.json eval_results/{impl}/{text_name}/{input_text}.out.json', shell=True)
        subprocess.run(f'mv ../tmp_output eval_results/{impl}/{text_name}/{input_text}.encoded', shell=True)

def run_full_eval(implementations: list, texts: list) -> None:
    run_make()
    for impl in implementations:
        run_single_opt_all(impl, texts)
    print("Done!")


def collect_results(implementations: list, texts: list) -> pandas.DataFrame:
    """
    Return a single DataFrame containing rows for ALL implementations × texts.
    """
    rows = []

    for text in texts:
        print(text)
        text_base = text.split(".")[0]
        print(text_base)
        for impl in implementations:
            json_path = f'eval_results/{impl}/{text_base}/{text}.out.json'
            with open(json_path, 'r') as f:
                data = json.load(f)

            rows.append({
                'Implementation': impl,
                'Text': text,                      # <-- Add text column to distinguish rows
                'Input File': data['input_file'],
                'Unique Chars': data['unique_chars'],
                'Total Chars': data['total_chars'],
                'Avg Compression Time (ms)': data['avg_compression_time_ms'],
                'Avg Decompression Time (ms)': data['avg_decompression_time_ms'],
                'File Size (bytes)': data['file_size_bytes'],
                'Compressed Size (bytes)': data['compressed_size_bytes'],
                'Compression Ratio': data['compression_ratio'],
                'Verified': data['verified']
            })

    return pandas.DataFrame(rows)

def save_results_to_csv(df: pandas.DataFrame, text: str) -> None:
    """
    Save *per-text* CSV.
    """
    text_name = text.split(".")[0]
    os.makedirs(f'eval_results/summary/{text_name}', exist_ok=True)

    filtered = df[df["Text"] == text]
    csv_path = f'eval_results/summary/{text_name}/{text_name}_summary.csv'
    filtered.to_csv(csv_path, index=False)

def plot_graph(df: pandas.DataFrame, text: str, logscale=False) -> None:
    text_name = text.split(".")[0]

    # Filter for only this text
    sub = df[df["Text"] == text]

    plt.figure(figsize=(10, 6))
    plt.bar(sub['Implementation'], sub['Avg Compression Time (ms)'], color='skyblue', edgecolor='black', width=0.4, log=logscale)
    plt.xlabel('Implementation')
    plt.ylabel('Avg Compression Time (ms)')
    plt.title(f'Avg Compression Time for {text}')
    plt.xticks(rotation=45)
    plt.tight_layout()

    os.makedirs(f'eval_results/summary/{text_name}', exist_ok=True)
    plot_path = f'eval_results/summary/{text_name}/{text_name}_compression_time.png'
    plt.savefig(plot_path)
    print(f"Saved plot to {plot_path}")

    # Decompression Time Plot
    plt.figure(figsize=(10, 6))
    plt.bar(sub['Implementation'], sub['Avg Decompression Time (ms)'], color='salmon', edgecolor='black', width=0.4, log=logscale)
    plt.xlabel('Implementation')
    plt.ylabel('Avg Decompression Time (ms)')
    plt.title(f'Avg Decompression Time for {text}')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plot_path = f'eval_results/summary/{text_name}/{text_name}_decompression_time.png'
    plt.savefig(plot_path)
    print(f"Saved plot to {plot_path}")


if __name__ == "__main__":
    implementations = ["rANS", "rANS_buf", "rANS_explicit", "rANS_explicit_opt", "rANS_thread", "rANS_implicit"]
    texts = ["sherlock_clean.txt", "carol_clean.txt", "shakespeare_clean.txt"]
    run_full_eval(implementations, texts)
    df = collect_results(implementations, texts)
    for text in texts:
        save_results_to_csv(df, text)
        plot_graph(df, text, logscale=True)
    print("All evaluation ran successfully.")