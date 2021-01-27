#include "thread.h"
#include "periph/uart.h"

#include "common.h"

#define SERIAL_MSG_QUEUE   (16U)
#define SERIAL_STACKSIZE   (THREAD_STACKSIZE_DEFAULT)
#define MSG_TYPE_ISR       (0x3456)

static char stack[SERIAL_STACKSIZE];
static kernel_pid_t serial_recv_pid;

static char uart_buffer[31];
static size_t uart_buffer_size;
static void _uart_rx_cb(void *arg, unsigned char data);
void *_serial_recv_thread(void *arg);

static forward_data_cb_t *serial_forwarder;

int serial_init(forward_data_cb_t *forwarder)
{
    serial_forwarder = forwarder;

    uart_buffer_size = 0;
    if (uart_init(UART_DEV(0), 115200, _uart_rx_cb, NULL) < 0) return 1;

    serial_recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, _serial_recv_thread, NULL,
                              "_serial_recv_thread");
    if (serial_recv_pid <= KERNEL_PID_UNDEF) return 1;

    return 0;
}

int serial_write(char *buffer, size_t len)
{
    uart_write(UART_DEV(0), (const uint8_t *)buffer, len);
    return len;
}

static void _uart_rx_cb(void *arg, unsigned char data)
{
    (void)arg;
    msg_t msg;
    msg.type = MSG_TYPE_ISR;
    msg.content.value = data;
    if (msg_send(&msg, serial_recv_pid) <= 0) {
       /* possibly lost interrupt */
    }
}

void *_serial_recv_thread(void *arg)
{
    (void)arg;
    static msg_t _msg_q[SERIAL_MSG_QUEUE];
    msg_init_queue(_msg_q, SERIAL_MSG_QUEUE);
    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            unsigned char data = msg.content.value;
            if (uart_buffer_size < sizeof(uart_buffer)) {
                uart_buffer[uart_buffer_size++] = (char)data;
            }
            if ((data == '\r') || (data == '\n') || (uart_buffer_size == (sizeof(uart_buffer)))) {
                serial_forwarder(uart_buffer, uart_buffer_size);
                uart_buffer_size = 0;
            }
        }
    }
}
