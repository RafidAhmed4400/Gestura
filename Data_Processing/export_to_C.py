import numpy as np
import tensorflow as tf


# ============================================================
# USER SETTINGS
# ============================================================

MODEL_PATH = "gesture_cnn_model.keras"
SCALER_MEAN_PATH = "scaler_mean.npy"
SCALER_STD_PATH = "scaler_std.npy"
CLASS_NAMES_PATH = "class_names.txt"

OUTPUT_C_FILE = "gesture_model_arrays_generated.c"


# ============================================================
# FORMAT HELPERS
# ============================================================

def format_float(x):
    return f"{x:.8f}f"


def write_1d_array(f, name, arr, c_size):
    arr = np.asarray(arr)
    f.write(f"static const float {name}[{c_size}] = {{\n")

    for i, value in enumerate(arr):
        comma = "," if i < len(arr) - 1 else ""
        f.write(f"    {format_float(value)}{comma}\n")

    f.write("};\n\n")


def write_2d_array(f, name, arr, c_dim0, c_dim1):
    arr = np.asarray(arr)
    dim0, dim1 = arr.shape

    f.write(f"static const float {name}[{c_dim0}][{c_dim1}] = {{\n")

    for i in range(dim0):
        f.write("    {")
        for j in range(dim1):
            comma = "," if j < dim1 - 1 else ""
            f.write(f"{format_float(arr[i, j])}{comma} ")
        comma_outer = "," if i < dim0 - 1 else ""
        f.write(f"}}{comma_outer}\n")

    f.write("};\n\n")


def write_3d_array(f, name, arr, c_dim0, c_dim1, c_dim2):
    arr = np.asarray(arr)
    dim0, dim1, dim2 = arr.shape

    f.write(f"static const float {name}[{c_dim0}][{c_dim1}][{c_dim2}] = {{\n")

    for i in range(dim0):
        f.write("    {\n")
        for j in range(dim1):
            f.write("        {")
            for k in range(dim2):
                comma = "," if k < dim2 - 1 else ""
                f.write(f"{format_float(arr[i, j, k])}{comma} ")
            comma_mid = "," if j < dim1 - 1 else ""
            f.write(f"}}{comma_mid}\n")
        comma_outer = "," if i < dim0 - 1 else ""
        f.write(f"    }}{comma_outer}\n")

    f.write("};\n\n")


def load_class_names(path):
    class_names = []

    with open(path, "r") as f:
        for line in f:
            idx, name = line.strip().split(",", 1)
            class_names.append(name)

    return class_names


def write_class_names(f, class_names):
    f.write(f"static const char *class_names[GM_NUM_CLASSES] = {{\n")

    for i, name in enumerate(class_names):
        comma = "," if i < len(class_names) - 1 else ""
        f.write(f'    "{name}"{comma}\n')

    f.write("};\n\n")


# ============================================================
# MAIN EXPORT
# ============================================================

def main():
    model = tf.keras.models.load_model(MODEL_PATH)

    scaler_mean = np.load(SCALER_MEAN_PATH).astype(np.float32)
    scaler_std = np.load(SCALER_STD_PATH).astype(np.float32)
    class_names = load_class_names(CLASS_NAMES_PATH)

    # Get layers by name. These names must match your training script.
    conv1 = model.get_layer("conv1")
    conv2 = model.get_layer("conv2")
    dense1 = model.get_layer("dense1")
    output = model.get_layer("output")

    conv1_w, conv1_b = conv1.get_weights()
    conv2_w, conv2_b = conv2.get_weights()
    dense1_w, dense1_b = dense1.get_weights()
    output_w, output_b = output.get_weights()

    # ------------------------------------------------------------
    # Keras Conv1D shape:
    #     [kernel_size][input_channels][filters]
    #
    # C expected shape:
    #     [filters][kernel_size][input_channels]
    # ------------------------------------------------------------
    conv1_w_c = np.transpose(conv1_w, (2, 0, 1))
    conv2_w_c = np.transpose(conv2_w, (2, 0, 1))

    # ------------------------------------------------------------
    # Keras Dense shape:
    #     [input_units][output_units]
    #
    # C expected shape:
    #     [output_units][input_units]
    # ------------------------------------------------------------
    dense1_w_c = dense1_w.T
    output_w_c = output_w.T

    # Basic shape checks
    print("Scaler mean shape:", scaler_mean.shape)
    print("Scaler std shape:", scaler_std.shape)
    print("conv1_w_c shape:", conv1_w_c.shape)
    print("conv1_b shape:", conv1_b.shape)
    print("conv2_w_c shape:", conv2_w_c.shape)
    print("conv2_b shape:", conv2_b.shape)
    print("dense1_w_c shape:", dense1_w_c.shape)
    print("dense1_b shape:", dense1_b.shape)
    print("output_w_c shape:", output_w_c.shape)
    print("output_b shape:", output_b.shape)
    print("classes:", class_names)

    with open(OUTPUT_C_FILE, "w") as f:
        f.write("// Auto-generated from trained Keras model.\n")
        f.write("// Copy these arrays into gesture_model.c, replacing the placeholder arrays.\n\n")

        f.write("// Make sure these macros in gesture_model.h / gesture_model.c match:\n")
        f.write("// GM_NUM_TIMESTEPS = 140\n")
        f.write("// GM_NUM_FEATURES = 15\n")
        f.write(f"// GM_NUM_CLASSES = {len(class_names)}\n\n")

        write_class_names(f, class_names)

        write_1d_array(f, "scaler_mean", scaler_mean, "GM_NUM_FEATURES")
        write_1d_array(f, "scaler_std", scaler_std, "GM_NUM_FEATURES")

        write_3d_array(
            f,
            "conv1_w",
            conv1_w_c,
            "CONV1_FILTERS",
            "CONV1_KERNEL",
            "GM_NUM_FEATURES"
        )

        write_1d_array(f, "conv1_b", conv1_b, "CONV1_FILTERS")

        write_3d_array(
            f,
            "conv2_w",
            conv2_w_c,
            "CONV2_FILTERS",
            "CONV2_KERNEL",
            "CONV1_FILTERS"
        )

        write_1d_array(f, "conv2_b", conv2_b, "CONV2_FILTERS")

        write_2d_array(
            f,
            "dense1_w",
            dense1_w_c,
            "DENSE1_UNITS",
            "CONV2_FILTERS"
        )

        write_1d_array(f, "dense1_b", dense1_b, "DENSE1_UNITS")

        write_2d_array(
            f,
            "output_w",
            output_w_c,
            "GM_NUM_CLASSES",
            "DENSE1_UNITS"
        )

        write_1d_array(f, "output_b", output_b, "GM_NUM_CLASSES")

    print(f"\nDone. Wrote C arrays to: {OUTPUT_C_FILE}")


if __name__ == "__main__":
    main()