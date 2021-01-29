#!/usr/bin/env -S python3 -u

import sys, serial, time

device = '/dev/ttyACM0'
baud = 115200
if len(sys.argv) > 1:
    device = sys.argv[1]
if len(sys.argv) > 2:
    baud = sys.argv[2]

s = serial.Serial(device, baud)
i = 0
while True:
    for c in range(32,127):
        i += 1
        s.write(bytes([c]))
        print(chr(c), end='')
        if i % 80 == 0:
            print('')
            i = 0
        time.sleep(.001)
