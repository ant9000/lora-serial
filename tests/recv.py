#!/usr/bin/env -S python3 -u

import serial

s = serial.Serial('/dev/ttyACM1', 115200, timeout=0)
i = 0
while True:
    c = s.read()
    if c:
        i += 1
        print(c.decode('utf8'), end='')
        if i % 80 == 0:
            print('')
            i = 0
