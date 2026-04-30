import serial

ser = serial.Serial('/dev/tty.usbserial-110', 115200)  # or 'COM#' for windows

with open("A_.csv", "w") as f:
    f.write("Blank, x, y, z, w, pinky, ring, middle, index, thumb, time\n")
    count = -1
    while True:
        line = ser.readline().decode().strip()
        if count > 0:
            f.write(line + ", " + str(count) + "\n")
        count += 1
