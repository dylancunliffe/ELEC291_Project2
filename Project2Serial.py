import serial
import readchar

# 1. Open the port
ser = serial.Serial('COM6', 115200, timeout=1)

manual_mode = 0

print("--- Script Started ---")
print("Click here and press 'm' to begin...")

while 1:
    strin = readchar.readkey() # Program PAUSES here until a key is hit
    
    if manual_mode == 1:
        if strin == 'm':
            manual_mode = 0
            print("Manual Mode OFF")
        elif strin in ['w', 'a', 's', 'd']:
            ser.write(strin.encode('utf-8'))
            print(f"Sent to uC: {strin}")
    else:
        if strin == 'm':
            manual_mode = 1
            print("Manual Mode ON - Ready for w,a,s,d")