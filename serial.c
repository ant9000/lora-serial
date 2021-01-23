#include "periph/uart.h"

#include "common.h"

static char uart_buffer[64];
static size_t uart_buffer_size;

static forward_data_cb_t *serial_forwarder;

int serial_init(forward_data_cb_t *forwarder)
{
    serial_forwarder = forwarder;

    uart_buffer_size = 0;
    if (uart_init(UART_DEV(0), 115200, serial_rx_cb, NULL) < 0) return 1;
    return 0;
}

size_t serial_write(char *buffer, size_t len)
{
    uart_write(UART_DEV(0), (const uint8_t *)buffer, len);
    return len;
}

void serial_rx_cb(void *arg, unsigned char data)
{
    (void)arg;

    if (uart_buffer_size < sizeof(uart_buffer)) {
        uart_buffer[uart_buffer_size++] = data;
    }
    if ((data == '\r') || (data == '\n') || (uart_buffer_size == sizeof(uart_buffer))) {
        if (serial_forwarder(uart_buffer, uart_buffer_size) == 0) {
            uart_buffer_size = 0;
        }
    }
}
