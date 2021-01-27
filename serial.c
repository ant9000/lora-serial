#include "thread.h"
#include "isrpipe.h"

#ifdef MODULE_USBUS_CDC_ACM
#include <sys/types.h>
#include "usb/usbus.h"
#include "usb/usbus/cdc/acm.h"
#ifdef MODULE_USB_BOARD_RESET
#include "usb_board_reset_internal.h"
#endif
#else
#include "periph/uart.h"
#endif

#include "common.h"

#define SERIAL_MSG_QUEUE   (16U)
#define SERIAL_STACKSIZE   (THREAD_STACKSIZE_DEFAULT)
#define MSG_TYPE_ISR       (0x3456)

static char stack[SERIAL_STACKSIZE];
static kernel_pid_t serial_recv_pid;

static uint8_t serial_buffer[31];
static size_t serial_buffer_count;
static uint8_t _serial_rx_buf_mem[128];
static isrpipe_t _serial_isrpipe = ISRPIPE_INIT(_serial_rx_buf_mem);
void *_serial_recv_thread(void *arg);

#ifdef MODULE_USBUS_CDC_ACM
static char usbus_stack[USBUS_STACKSIZE];
static usbus_t usbus;
static usbus_cdcacm_device_t cdcacm;
static uint8_t _cdc_tx_buf_mem[CONFIG_USBUS_CDC_ACM_STDIO_BUF_SIZE];
static void _cdc_acm_rx_pipe(usbus_cdcacm_device_t *cdcacm, uint8_t *data, size_t len);
#else
static void _uart_rx_cb(void *arg, unsigned char data);
#endif

static forward_data_cb_t *serial_forwarder;

int serial_init(forward_data_cb_t *forwarder)
{
    serial_forwarder = forwarder;
    serial_buffer_count = 0;

#ifdef MODULE_USBUS_CDC_ACM
    usbdev_t *usbdev = usbdev_get_ctx(0);
    usbus_init(&usbus, usbdev);
    usbus_cdc_acm_init(&usbus, &cdcacm, _cdc_acm_rx_pipe, NULL,
                       _cdc_tx_buf_mem, sizeof(_cdc_tx_buf_mem));
    usbus_create(usbus_stack, USBUS_STACKSIZE, USBUS_PRIO, USBUS_TNAME, &usbus);
#else
    if (uart_init(UART_DEV(0), 115200, _uart_rx_cb, NULL) < 0) return 1;
#endif

    serial_recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, _serial_recv_thread, NULL,
                              "_serial_recv_thread");
    if (serial_recv_pid <= KERNEL_PID_UNDEF) return 1;

    return 0;
}

int serial_write(char *buffer, size_t len)
{
#ifdef MODULE_USBUS_CDC_ACM
    /* TODO: flush full buffer */
    const char *start = buffer;
    do {
        size_t n = usbus_cdc_acm_submit(&cdcacm, (const void *)buffer, len);
        usbus_cdc_acm_flush(&cdcacm);
        /* Use tsrb and flush */
        buffer = buffer + n;
        len -= n;
    } while (len);
    return buffer - start;
#else
    uart_write(UART_DEV(0), (const uint8_t *)buffer, len);
    return len;
#endif
}

#ifdef MODULE_USBUS_CDC_ACM
static void _cdc_acm_rx_pipe(usbus_cdcacm_device_t *cdcacm,
                             uint8_t *data, size_t len)
{
    (void)cdcacm;
    for (size_t i = 0; i < len; i++) {
        isrpipe_write_one(&_serial_isrpipe, data[i]);
    }
    msg_t msg;
    msg.type = MSG_TYPE_ISR;
    msg.content.value = len;
    if (msg_send(&msg, serial_recv_pid) <= 0) {
       /* possibly lost interrupt */
    }
}
#else
static void _uart_rx_cb(void *arg, unsigned char data)
{
    (void)arg;
    isrpipe_write_one(&_serial_isrpipe, data);
    msg_t msg;
    msg.type = MSG_TYPE_ISR;
    msg.content.value = 1;
    if (msg_send(&msg, serial_recv_pid) <= 0) {
       /* possibly lost interrupt */
    }
}
#endif

void *_serial_recv_thread(void *arg)
{
    (void)arg;
    static msg_t _msg_q[SERIAL_MSG_QUEUE];
    msg_init_queue(_msg_q, SERIAL_MSG_QUEUE);
    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            size_t len = msg.content.value;
            while (len > 0) {
                size_t serial_buffer_free = sizeof(serial_buffer) - serial_buffer_count;
                size_t n = len < serial_buffer_free ? len : serial_buffer_free;
                uint8_t *start = serial_buffer + serial_buffer_count;
                isrpipe_read(&_serial_isrpipe, start, n);
                serial_buffer_count += n;
                if ((start[n-1] == '\r') || (start[n] == '\n') || (serial_buffer_count == sizeof(serial_buffer))) {
                    serial_forwarder((char *)serial_buffer, serial_buffer_count);
                    serial_buffer_count = 0;
                }
                len -= n;
            }
        }
    }
}
