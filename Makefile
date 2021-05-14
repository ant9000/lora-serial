APPLICATION = lora-serial
BOARD ?= lora3a-dongle
RIOTBASE ?= $(CURDIR)/../RIOT
EXTERNAL_BOARD_DIRS ?= $(CURDIR)/../lora3a-boards/boards
QUIET ?= 1
DEVELHELP ?= 1
SERIAL_INTERFACE ?= usb

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

ifeq (usb,$(SERIAL_INTERFACE))
  USEMODULE += stdio_cdc_acm
  CFLAGS += -DCONFIG_USB_MAX_POWER=500
  CFLAGS += -DCONFIG_USBUS_CDC_ACM_STDIO_BUF_SIZE=4096
else
  USEMODULE += stdio_uart
  USEMODULE += stdio_uart_rx
  CFLAGS += -DSTDIO_UART_RX_BUFSIZE=4096
endif

include $(RIOTBASE)/Makefile.include
