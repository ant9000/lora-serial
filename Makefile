APPLICATION = lora-serial
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../riot
QUIET ?= 1
DEVELHELP ?= 1
#SERIAL_INTERFACE ?= usb

USEMODULE += periph_hwrng
USEMODULE += sx1276
USEMODULE += xtimer
USEMODULE += isrpipe

CFLAGS += -DCONFIG_SKIP_BOOT_MSG=1

ifeq (usb,$(SERIAL_INTERFACE))
  USEMODULE += auto_init_usbus
  USEMODULE += stdio_cdc_acm

  # USB device vendor and product ID
  DEFAULT_VID = 1209
  DEFAULT_PID = 0001
  USB_VID ?= $(DEFAULT_VID)
  USB_PID ?= $(DEFAULT_PID)

  CFLAGS += -DUSB_CONFIG_VID=0x$(USB_VID) -DUSB_CONFIG_PID=0x$(USB_PID)
  CFLAGS += -DCONFIG_USB_MAX_POWER=500
else
  USEMODULE += stdio_uart
  USEMODULE += stdio_uart_rx
endif

include $(RIOTBASE)/Makefile.include
