from pathlib import Path
import numpy as np
import pandas as pd
import tensorflow as tf

from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
from sklearn.metrics import classification_report, confusion_matrix


# ============================================================
# USER SETTINGS
# ============================================================

DATA_ROOT = Path(r"C:\Users\thero\Gestura_project\Data_Processing\split_data")

TIMESTEPS = 140
NUM_FEATURES = 11

BATCH_SIZE = 32
EPOCHS = 40
TEST_SIZE = 0.20
RANDOM_STATE = 42

MODEL_SAVE_PATH = "gesture_cnn_model.keras"


# ============================================================
# EXPECTED FEATURE ORDER
# ============================================================
# Your original files have useless first and last columns:
#   first column:  Blank
#   last column:   index_1
#
# After removing those, the remaining 15 columns should be:
#
# g_x, g_y, g_z,
# a_x, a_y, a_z,
# x, y, z, w,
# pinky, ring, middle, index, thumb
#
# This must match the ESP32 C buffer order exactly.

EXPECTED_FEATURE_COLUMNS = [
    "g_x", "g_y", "g_z",
    "a_x", "a_y", "a_z",
    # "x", "y", "z", "w",
    "pinky", "ring", "middle", "index", "thumb"
]


# ============================================================
# LOAD ONE CSV FILE
# ============================================================

def load_one_csv(csv_path: Path):
    """
    Loads one gesture CSV file.

    Label is taken from the parent folder name.
    Example:
        split_data/A/A_1_window_01.csv
        label = A

    The first and last columns are dropped.
    The remaining data should be 140 rows x 15 features.
    """

    df = pd.read_csv(csv_path)

    FEATURE_COLS = [
        "g_x", "g_y", "g_z",
        "a_x", "a_y", "a_z",
        # "x", "y", "z", "w",
        "pinky", "ring", "middle", "index", "thumb"
    ]
    df.columns = df.columns.str.strip()
    df = df[FEATURE_COLS]

    if df.shape[0] != TIMESTEPS:
        raise ValueError(
            f"{csv_path} has {df.shape[0]} rows, expected {TIMESTEPS}."
        )

    # if df.shape[1] != NUM_FEATURES:
    #     raise ValueError(
    #         f"{csv_path} has {df.shape[1]} features after dropping first/last columns, "
    #         f"expected {NUM_FEATURES}."
    #     )
    if df.isna().any().any():
        print("\nNaN problem in file:")
        print(csv_path)

        print("\nNaN count per column:")
        print(df.isna().sum())

        print("\nFirst 5 rows:")
        print(df.head())

        raise ValueError("Stopping because this file contains NaN values.")

    # Optional safety check:
    # This checks if column names match what we expect.
    actual_columns = list(df.columns)

    if actual_columns != EXPECTED_FEATURE_COLUMNS:
        print(f"\nWARNING: Column order mismatch in {csv_path}")
        print("Expected:")
        print(EXPECTED_FEATURE_COLUMNS)
        print("Actual:")
        print(actual_columns)
        print("Continuing anyway, but make sure the order is correct.\n")

    # Convert to float32 for TensorFlow
    x = df.values.astype(np.float32)

    # Label comes from folder name
    label = csv_path.relative_to(DATA_ROOT).parts[0]

    return x, label


# ============================================================
# LOAD ALL CSV FILES
# ============================================================

def load_dataset(data_root: Path):
    """
    Recursively finds all CSV files inside DATA_ROOT.
    Loads each CSV as one training example.
    """

    csv_files = sorted(data_root.rglob("*.csv"))

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found inside {data_root}")

    X = []
    y = []

    print(f"Found {len(csv_files)} CSV files.")

    for csv_path in csv_files:
        try:
            x_sample, label = load_one_csv(csv_path)
            X.append(x_sample)
            y.append(label)
        except Exception as e:
            print(f"Skipping {csv_path}")
            print(f"Reason: {e}")

    X = np.array(X, dtype=np.float32)
    y = np.array(y)

    print("\nDataset loaded.")
    print(f"X shape: {X.shape}")
    print(f"y shape: {y.shape}")
    print(f"Labels found: {sorted(set(y))}")

    return X, y


# ============================================================
# NORMALIZE DATA
# ============================================================

def normalize_data(X_train, X_test):
    """
    Computes mean and std from training data only.

    The mean/std are calculated per feature across all samples and timesteps.

    X_train shape:
        num_examples x 140 x 15

    scaler_mean shape:
        15

    scaler_std shape:
        15
    """

    scaler_mean = X_train.mean(axis=(0, 1))
    scaler_std = X_train.std(axis=(0, 1))

    # Prevent division by zero
    scaler_std[scaler_std == 0] = 1.0

    X_train_norm = (X_train - scaler_mean) / scaler_std
    X_test_norm = (X_test - scaler_mean) / scaler_std

    return X_train_norm, X_test_norm, scaler_mean, scaler_std


# ============================================================
# BUILD CNN MODEL
# ============================================================

def build_model(num_classes):
    """
    This architecture is designed to match the C inference file.

    C model equivalent:
        Input: 140 x 15
        Conv1D: 8 filters, kernel 5, same padding, ReLU
        MaxPool1D: pool size 2
        Conv1D: 16 filters, kernel 3, same padding, ReLU
        GlobalAveragePooling1D
        Dense: 16, ReLU
        Dense: num_classes, Softmax
    """

    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(TIMESTEPS, NUM_FEATURES)),

        tf.keras.layers.Conv1D(
            filters=8,
            kernel_size=5,
            padding="same",
            activation="relu",
            name="conv1"
        ),

        tf.keras.layers.MaxPooling1D(
            pool_size=2,
            name="maxpool1"
        ),

        tf.keras.layers.Conv1D(
            filters=16,
            kernel_size=3,
            padding="same",
            activation="relu",
            name="conv2"
        ),

        tf.keras.layers.GlobalAveragePooling1D(
            name="global_avg_pool"
        ),

        tf.keras.layers.Dense(
            16,
            activation="relu",
            name="dense1"
        ),

        tf.keras.layers.Dense(
            num_classes,
            activation="softmax",
            name="output"
        )
    ])

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"]
    )

    return model


# ============================================================
# EXPORT SCALER VALUES FOR C
# ============================================================

def print_c_array(name, values):
    """
    Prints a 1D numpy array as a C float array.
    """

    print(f"\nstatic const float {name}[{len(values)}] = {{")

    for i, value in enumerate(values):
        comma = "," if i < len(values) - 1 else ""
        print(f"    {value:.8f}f{comma}")

    print("};")


# ============================================================
# MAIN
# ============================================================

def main():
    # ----------------------------
    # Load dataset
    # ----------------------------
    X, y_text = load_dataset(DATA_ROOT)

    # ----------------------------
    # Encode labels
    # Example:
    # A -> 0
    # B -> 1
    # C -> 2
    # ----------------------------
    label_encoder = LabelEncoder()
    y = label_encoder.fit_transform(y_text)

    class_names = list(label_encoder.classes_)
    num_classes = len(class_names)

    print("\nClass mapping:")
    for idx, name in enumerate(class_names):
        print(f"{idx}: {name}")

    print(f"\nNumber of classes: {num_classes}")

    # ----------------------------
    # Train/test split
    # ----------------------------
    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y,
        test_size=TEST_SIZE,
        random_state=RANDOM_STATE,
        stratify=y
    )

    # ----------------------------
    # Normalize
    # ----------------------------
    X_train_norm, X_test_norm, scaler_mean, scaler_std = normalize_data(
        X_train,
        X_test
    )

    print("\nScaler values for C:")
    print_c_array("scaler_mean", scaler_mean)
    print_c_array("scaler_std", scaler_std)

    # ----------------------------
    # Build model
    # ----------------------------
    model = build_model(num_classes)
    model.summary()

    # ----------------------------
    # Train model
    # ----------------------------
    history = model.fit(
        X_train_norm,
        y_train,
        validation_data=(X_test_norm, y_test),
        batch_size=BATCH_SIZE,
        epochs=EPOCHS,
        verbose=1
    )

    # ----------------------------
    # Evaluate
    # ----------------------------
    test_loss, test_acc = model.evaluate(X_test_norm, y_test, verbose=0)

    print("\nFinal test results:")
    print(f"Loss: {test_loss:.4f}")
    print(f"Accuracy: {test_acc:.4f}")

    # ----------------------------
    # Predictions
    # ----------------------------
    y_pred_probs = model.predict(X_test_norm)
    y_pred = np.argmax(y_pred_probs, axis=1)

    print("\nClassification report:")
    print(classification_report(
        y_test,
        y_pred,
        target_names=class_names
    ))

    print("\nConfusion matrix:")
    print(confusion_matrix(y_test, y_pred))

    # ----------------------------
    # Save model
    # ----------------------------
    model.save(MODEL_SAVE_PATH)
    print(f"\nSaved trained model to: {MODEL_SAVE_PATH}")

    # ----------------------------
    # Save class names
    # ----------------------------
    with open("class_names.txt", "w") as f:
        for idx, name in enumerate(class_names):
            f.write(f"{idx},{name}\n")

    print("Saved class names to: class_names.txt")

    # ----------------------------
    # Save scaler values
    # ----------------------------
    np.save("scaler_mean.npy", scaler_mean)
    np.save("scaler_std.npy", scaler_std)

    print("Saved scaler_mean.npy and scaler_std.npy")


if __name__ == "__main__":
    main()