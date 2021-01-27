#!/usr/bin/env -S python3 -u

import serial, time

s = serial.Serial('/dev/ttyACM0', 115200)
i = 0
while True:
    for c in range(32,127):
        i += 1
        s.write(bytes([c]))
        print(chr(c), end='')
        if i % 80 == 0:
            print('')
            i = 0
        time.sleep(.01)
