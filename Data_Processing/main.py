from pathlib import Path
import pandas as pd


INPUT_ROOT = Path(r"C:\Users\thero\Gestura_project\Data_Processing\Data")        # Main folder containing your 13 folders
OUTPUT_ROOT = Path(r"C:\Users\thero\Gestura_project\Data_Processing\Split_Data")       # Output folder
ROWS_PER_FILE = 140                    # Rows per split CSV
NUM_SPLITS_PER_CSV = 10                # 1400 rows / 140 = 10 files


# Main

def split_one_csv(csv_path: Path, input_root: Path, output_root: Path):
    """
    Splits one CSV file into multiple smaller CSV files.
    Preserves the folder structure from input_root inside output_root.
    """

    df = pd.read_csv(csv_path)

    # Figure out where this file lives relative to INPUT_ROOT
    relative_path = csv_path.relative_to(input_root)

    # Example:
    # raw_data/A/trial1.csv
    # becomes:
    # split_data/A/trial1/
    output_folder = output_root / relative_path.parent / csv_path.stem
    output_folder.mkdir(parents=True, exist_ok=True)

    files_created = 0

    for split_idx in range(NUM_SPLITS_PER_CSV):
        start_row = split_idx * ROWS_PER_FILE
        end_row = start_row + ROWS_PER_FILE

        chunk = df.iloc[start_row:end_row]

        if len(chunk) < ROWS_PER_FILE:
            print(
                f"Skipping remaining rows in {csv_path}: "
                f"only {len(chunk)} rows left, need {ROWS_PER_FILE}."
            )
            break

        output_name = f"{csv_path.stem}_window_{split_idx + 1:02d}.csv"
        output_path = output_folder / output_name

        # index= False keeps original header and avoids adding pandas index column
        chunk.to_csv(output_path, index=False)

        files_created += 1

    print(f"Processed {csv_path} → created {files_created} files")


def split_all_csvs(input_root: Path, output_root: Path):
    input_root = input_root.resolve()
    output_root = output_root.resolve()

    if not input_root.exists():
        raise FileNotFoundError(f"Input folder does not exist: {input_root}")

    csv_files = list(input_root.rglob("*.csv"))

    if not csv_files:
        print(f"No CSV files found inside {input_root}")
        return

    print(f"Found {len(csv_files)} CSV files.")

    for csv_path in csv_files:
        split_one_csv(csv_path, input_root, output_root)

    print("\nDone splitting all CSV files.")


if __name__ == "__main__":
    split_all_csvs(INPUT_ROOT, OUTPUT_ROOT)