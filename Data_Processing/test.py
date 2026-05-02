from pathlib import Path
import numpy as np
import pandas as pd
import tensorflow as tf


# =========================
# USER SETTINGS
# =========================

MODEL_PATH = "gesture_cnn_model.keras"
SCALER_MEAN_PATH = "scaler_mean.npy"
SCALER_STD_PATH = "scaler_std.npy"
CLASS_NAMES_PATH = "class_names.txt"

CSV_PATH = Path(r"C:\Users\thero\Gestura_project\Data_Processing\Split_Data\Test\test_A.csv")

TIMESTEPS = 140
NUM_FEATURES = 15

FEATURE_COLS = [
    "g_x", "g_y", "g_z",
    "a_x", "a_y", "a_z",
    "x", "y", "z", "w",
    "pinky", "ring", "middle", "index", "thumb"
]

# Load class names

def load_class_names(path):
    class_names = []

    with open(path, "r") as f:
        for line in f:
            idx, name = line.strip().split(",", 1)
            class_names.append(name)

    return class_names


# Load the csv file

def load_one_csv(csv_path):
    df = pd.read_csv(csv_path)
    df.columns = df.columns.str.strip()

    df = df[FEATURE_COLS]
    df = df.apply(pd.to_numeric, errors="coerce")

    if df.isna().any().any():
        raise ValueError(f"NaN found in file:\n{df.isna().sum()}")

    if df.shape != (TIMESTEPS, NUM_FEATURES):
        raise ValueError(f"Expected {(TIMESTEPS, NUM_FEATURES)}, got {df.shape}")

    x = df.values.astype(np.float32)
    return x


# Main

model = tf.keras.models.load_model(MODEL_PATH)
scaler_mean = np.load(SCALER_MEAN_PATH)
scaler_std = np.load(SCALER_STD_PATH)
class_names = load_class_names(CLASS_NAMES_PATH)

x = load_one_csv(CSV_PATH)

# Normalize exactly like training
x_norm = (x - scaler_mean) / scaler_std

# Add batch dimension: (1, 140, 15)
x_norm = np.expand_dims(x_norm, axis=0)

probs = model.predict(x_norm)[0]

pred_idx = int(np.argmax(probs))
pred_label = class_names[pred_idx]
confidence = probs[pred_idx]

print("\nPrediction result:")
print(f"File: {CSV_PATH}")
print(f"Predicted class: {pred_label}")
print(f"Confidence: {confidence:.4f}")

print("\nAll probabilities:")
for name, prob in zip(class_names, probs):
    print(f"{name}: {prob:.4f}")