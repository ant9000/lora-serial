APPLICATION = lora-serial
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../riot
QUIET ?= 1
DEVELHELP ?= 1

USEMODULE += periph_hwrng
USEMODULE += stdio_null
USEMODULE += sx1276
USEMODULE += xtimer

ifeq (usb,$(SERIAL_INTERFACE))
  USEMODULE += usbus_cdc_acm

  # USB device vendor and product ID
  DEFAULT_VID = 1209
  DEFAULT_PID = 0001
  USB_VID ?= $(DEFAULT_VID)
  USB_PID ?= $(DEFAULT_PID)

  CFLAGS += -DUSB_CONFIG_VID=0x$(USB_VID) -DUSB_CONFIG_PID=0x$(USB_PID)
else
  USEMODULE += periph_uart
endif

include $(RIOTBASE)/Makefile.include
