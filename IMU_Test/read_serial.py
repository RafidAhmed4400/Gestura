import serial

ser = serial.Serial('/dev/tty.usbserial-110', 115200)  # or 'COM#' for windows

with open("imu_data.csv", "w") as f:
    while True:
        line = ser.readline().decode().strip()
        f.write(line + "\n")
