#!/usr/bin/env -S python3 -u

import sys, serial

device = '/dev/ttyACM1'
baud = 115200
if len(sys.argv) > 1:
    device = sys.argv[1]
if len(sys.argv) > 2:
    baud = sys.argv[2]

s = serial.Serial(device, baud, timeout=0)
i = 0
try:
    while True:
        c = s.read()
        if c:
            i += 1
            print(c.decode('utf8'), end='')
            if i % 80 == 0:
                print('')
except KeyboardInterrupt:
    print("")
    print("Received %d characters." % i)
