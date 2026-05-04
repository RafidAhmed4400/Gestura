import serial
import time

start = time.time()


ser = serial.Serial('/dev/tty.usbserial-110', 115200)  # or 'COM#' for windows

with open("gesture_data_gravity/Z/Z_50.csv", "w") as f:
    f.write("Blank, g_x, g_y, g_z, a_x, a_y, a_z, grav_x, grav_y, grav_z, x, y, z, w, pinky, ring, middle, index, thumb, tick\n")
    count = -2
    while True:
        line = ser.readline().decode().strip()
        if count > 0:
            f.write(line + ", " + str(count) + "\n")
        count += 1
        if time.time() - start >= 1.5:
            break
