#include "thread.h"
#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "net/lora.h"

#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "common.h"

#define SX127X_LORA_MSG_QUEUE   (128U)
#define SX127X_STACKSIZE        (THREAD_STACKSIZE_DEFAULT)
#define MSG_TYPE_ISR            (0x3456)

static char stack[SX127X_STACKSIZE];
static kernel_pid_t lora_recv_pid;
static sx127x_t sx127x;
static char lora_buffer[MAX_PACKET_LEN];

static forward_data_cb_t *lora_forwarder;
static void _lora_rx_cb(netdev_t *dev, netdev_event_t event);
void *_lora_recv_thread(void *arg);

int lora_init(lora_state_t *state, forward_data_cb_t *forwarder)
{
    lora_forwarder = forwarder;

    sx127x.params = sx127x_params[0];
    netdev_t *netdev = (netdev_t *)&sx127x;
    netdev->driver = &sx127x_driver;

    if (netdev->driver->init(netdev) < 0) return 1;

    uint8_t lora_bw = LORA_BW_500_KHZ;
    uint8_t lora_sf = LORA_SF7;
    uint8_t lora_cr = LORA_CR_4_5;
    uint32_t lora_ch = SX127X_CHANNEL_DEFAULT;
    if (state != NULL) {
        lora_bw = state->bandwidth;
        lora_sf = state->spreading_factor;
        lora_cr = state->coderate;
        lora_ch = state->channel;
    }

    netdev->driver->set(netdev, NETOPT_BANDWIDTH, &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR, &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE, &lora_cr, sizeof(lora_cr));
    netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &lora_ch, sizeof(lora_ch));

    netdev->event_callback = _lora_rx_cb;

    lora_recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, _lora_recv_thread, NULL,
                              "_lora_recv_thread");

    if (lora_recv_pid <= KERNEL_PID_UNDEF) return 1;
    return 0;
}

int lora_write(char *msg, size_t len)
{
    iolist_t payload = {
        .iol_base = msg,
        .iol_len = len,
    };
    netdev_t *netdev = (netdev_t *)&sx127x;
    while (netdev->driver->send(netdev, &payload) == -ENOTSUP) { xtimer_usleep64(100); }
    return len;
}

void lora_listen(void)
{
    netdev_t *netdev = (netdev_t *)&sx127x;
    const netopt_enable_t single = false;
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 0;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));
    netopt_state_t state = NETOPT_STATE_RX;
    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));
}

static void _lora_rx_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg;
        msg.type = MSG_TYPE_ISR;
        msg.content.ptr = dev;
        if (msg_send(&msg, lora_recv_pid) <= 0) {
           /* possibly lost interrupt */
        }
    } else {
        size_t len, n;
        switch (event) {
            case NETDEV_EVENT_RX_COMPLETE:
                len = dev->driver->recv(dev, NULL, 0, 0);
                while (len > 0) {
                    n = len < sizeof(lora_buffer) ? len : sizeof(lora_buffer);
                    dev->driver->recv(dev, lora_buffer, n, NULL);
                    lora_forwarder(lora_buffer, n);
                    len -= n;
                }
                break;
            case NETDEV_EVENT_TX_COMPLETE:
                lora_listen();
                break;
            default:
                break;
        }
    }
}

void *_lora_recv_thread(void *arg)
{
    (void)arg;
    static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];
    msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);
    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
    }
}
