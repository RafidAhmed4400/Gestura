import numpy as np

scaler_mean = np.load("scaler_mean.npy")
scaler_std = np.load("scaler_std.npy")

print("scaler_mean:")
print(scaler_mean)

print("\nscaler_std:")
print(scaler_std)

print("\nShapes:")
print("scaler_mean shape:", scaler_mean.shape)
print("scaler_std shape:", scaler_std.shape)