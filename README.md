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

CONFIGURATION
-------------

Default LoRa radio parameters, as well as a default 128 bit key for AES, are compiled statically in the firmware in order to have a working serial link right after flashing, even with no configuration. Of course, using the default AES key in production nullifies the protection offered by encryption.

A runtime configuration mode is accessible by turning on the board with BTN0 pressed. Using any serial emulator, it will be possible to configure interactively all the needed parameters, and then save them to the SAML21 EEPROM.

Type _help_ in configuration mode to see the list of available commands.
