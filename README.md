DESCRIPTION
-----------

This RIOT application offers serial port remoting via LoRa radio link; it is designed for samr34-xpro boards. Flash two boards and connect them to two PCs serial ports: the radio link is used as an RF serial connection. All the traffic is encrypted and validated using hardware accelerated AES-GCM-128 block cipher mode encryption.

By default the first UART will be used as the communication port, but the firmware can be compiled to use the board USB interface instead.

INSTALLATION
------------

```
git clone https://github.com/RIOT-OS/RIOT riot
git clone https://github.com/ant9000/lora-serial
cd lora-serial
```

USAGE
-----

To compile a firmware that forwards UART(0) via LoRA:

```
make flash
```

In case you want to forward the USB CDC ACM port:

```
make flash SERIAL_INTERFACE=usb
```

The chosen serial port is configured as 115200 8N1. On Linux it could be accessed, for instance, with:

```
minicom -D /dev/ttyACM0
```

(substitute the device node needed for your machine).

TODO
----

At present the LoRa radio parameters, as well as the 128 bit key for AES, are compiled statically in the firmware.

I plan to implement a runtime configuration mode, accessible by turning on the board with BTN0 pressed. It will offer a shell for configuring all the needed parameters and saving them to EEPROM.
