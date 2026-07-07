import pandas as pd


def load_dataset(csv_path):
    df = pd.read_csv(csv_path)

    expected_columns = [
        "timestamp_us",
        "ir_raw",
        "red_raw",
        "acc_x",
        "acc_y",
        "acc_z",
        "gyro_x",
        "gyro_y",
        "gyro_z",
    ]

    if all(col in df.columns for col in expected_columns):
        return df

    raise ValueError(f"Wrong CSV format. Columns: {df.columns.tolist()}")