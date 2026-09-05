import serial
import time

ser = serial.Serial('COM8', 115200, timeout=1)
print("Triggering ESP32 Reset via DTR/RTS...")
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.1)
ser.setRTS(False)
time.sleep(0.1)

t0 = time.time()
while time.time() - t0 < 6:
    line = ser.readline().decode('utf-8', errors='ignore')
    if line:
        print(line, end='')

ser.close()
