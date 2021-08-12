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

To compile a firmware that forwards CDC ACM port via LoRA:

```
make flash
```

The serial port is configured as 115200 8N1. On Linux it could be accessed, for instance, with:

```
minicom -D /dev/ttyACM0
```

(substitute the device node needed for your machine).

CONFIGURATION
-------------

Default LoRa radio parameters, as well as a default 128 bit key for AES, are compiled statically in the firmware in order to have a working serial link right after flashing, even with no configuration. Of course, using the default AES key in production nullifies the protection offered by encryption.

A runtime configuration mode is accessible by turning on the board with BTN0 pressed. Using any serial emulator, it will be possible to configure interactively all the needed parameters, and then save them to the SAML21 EEPROM.

Type _help_ in configuration mode to see the list of available commands.


EXAMPLE
-------

Connect one LoRa module to Linux machine `pc1`; let's say it appears as `/dev/ttyACM0`. Launch a terminal listener on that serial port, with

```sudo agetty ttyACM0 115200```

Now connect the second LoRa module to another Linux board `pc2`; suppose it is available as `/dev/ttyACM0` there too. Then with

```screen /dev/ttyACM0 115200```

from `pc2` you will obtain a remote login shell to `pc1`, over the LoRa link.


RIOTBOOT
--------

It is possible to program the modules with Riotboot DFU bootloader: successive firmware upgrades can then be carried out using USB only. The bootloader is programmed like this:

```
BOARD=samr34-xpro make -C ../riot/bootloaders/riotboot_dfu/ all flash
```

When the board has the bootloader in place, you can produce and flash a new firmware via USB:

```
FEATURES_REQUIRED+=riotboot USEMODULE+=usbus_dfu PROGRAMMER=dfu-util SERIAL_INTERFACE=usb make riotboot/flash-slot0
```

or, making use of the alternate slot:

```
FEATURES_REQUIRED+=riotboot USEMODULE+=usbus_dfu PROGRAMMER=dfu-util SERIAL_INTERFACE=usb make riotboot/flash-slot1 DFU_ALT=1
```

Please note that the `dfu-util` tool is not part of RIOT and must be installed separately. Also, on Linux you need to make sure that the USB nodes used for communicating with your device are accessible to your user. Assuming you are a member of `plugdev` group, execute the following commands as root:

```
cat >/etc/udev/rules.d/99-riot.rules <<EOF
# default VID:PID
ATTRS{idVendor}=="1209", ATTRS{idProduct}=="0001", MODE="664", GROUP="plugdev"
# DFU mode
ATTRS{idVendor}=="1209", ATTRS{idProduct}=="7d00", MODE="664", GROUP="plugdev"
ATTRS{idVendor}=="1209", ATTRS{idProduct}=="7d02", MODE="664", GROUP="plugdev"
EOF
udevadm control --reload-rules
udevadm trigger
```
