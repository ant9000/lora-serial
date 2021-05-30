APPLICATION = lora-serial
BOARD ?= lora3a-dongle
RIOTBASE ?= $(CURDIR)/../RIOT
EXTERNAL_BOARD_DIRS ?= $(CURDIR)/../lora3a-boards/boards
QUIET ?= 1
DEVELHELP ?= 1

USEMODULE += periph_hwrng
USEMODULE += periph_flashpage
USEMODULE += sx1276
USEMODULE += xtimer
USEMODULE += isrpipe
USEMODULE += fmt
USEMODULE += od
USEMODULE += od_string
USEMODULE += shell

CFLAGS += -DCONFIG_SKIP_BOOT_MSG=1

USEMODULE += stdio_cdc_acm
CFLAGS += -DCONFIG_USB_MAX_POWER=500
CFLAGS += -DCONFIG_USBUS_CDC_ACM_STDIO_BLOCKING=1

include $(RIOTBASE)/Makefile.include
