APPLICATION = lora-serial
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../riot
QUIET ?= 1
DEVELHELP ?= 1

USEMODULE += stdio_null
USEMODULE += sx1276
USEMODULE += xtimer

#USEMODULE += auto_init_usbus
#USEMODULE += usbus_cdc_acm
#DEFAULT_VID = 1209
#DEFAULT_PID = 0001
#USB_VID ?= $(DEFAULT_VID)
#USB_PID ?= $(DEFAULT_PID)
#CFLAGS += -DCONFIG_USB_VID=0x$(USB_VID) -DCONFIG_USB_PID=0x$(USB_PID)

include $(RIOTBASE)/Makefile.include
