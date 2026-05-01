import serial
import time

start = time.time()


ser = serial.Serial('/dev/tty.usbserial-110', 115200)  # or 'COM#' for windows

with open("N_.csv", "w") as f:
    f.write("Blank, g_x, g_y, g_z, a_x, a_y, a_z, x, y, z, w, pinky, ring, middle, index, thumb, index\n")
    count = -1
    while True:
        line = ser.readline().decode().strip()
        if count > 0:
            f.write(line + ", " + str(count) + "\n")
        count += 1
        if time.time() - start >= 16:
            break
