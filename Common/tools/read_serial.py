import serial
import time

try:
    ser = serial.Serial('COM8', 115200, timeout=1)
    print("Connected to COM8")
    t0 = time.time()
    while time.time() - t0 < 6:
        line = ser.readline().decode('utf-8', errors='ignore')
        if line:
            print(line, end='')
    ser.close()
except Exception as e:
    print(f"Error: {e}")
