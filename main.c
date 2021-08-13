#include <string.h>
#include "fmt.h"
#include "stdio_base.h"
#include "board.h"
#include "thread.h"

#include "periph/hwrng.h"
#include "periph/gpio.h"

#include "common.h"
#include "configuration.h"
#include "protocol.h"
#include "aes.h"

serialized_state_t state;
struct aes_sync_device aes;
kernel_pid_t pid_main;
consume_data_cb_t *packet_consumer;

static bool loop=1;
static void timeout_cb(void *arg)
{
    (void)arg;
    if (loop) {
        loop=0;
        LED1_ON;
    }
}
static ztimer_t timeout = { .callback = timeout_cb };
static void btn_cb(void *arg)
{
    (void)arg;
    if (gpio_read(BTN0_PIN) == 0) {
        ztimer_set(ZTIMER_MSEC, &timeout, 2500);
    } else {
        ztimer_remove(ZTIMER_MSEC, &timeout);
    }
}

int main(void)
{
    uint8_t serial_buffer[63];
    size_t serial_buffer_count;

    if (load_from_flash (&state, sizeof(state)) < 0) {
        memset(&state, 0, sizeof(state));
        state.lora.bandwidth        = DEFAULT_LORA_BANDWIDTH;
        state.lora.spreading_factor = DEFAULT_LORA_SPREADING_FACTOR;
        state.lora.coderate         = DEFAULT_LORA_CODERATE;
        state.lora.channel          = DEFAULT_LORA_CHANNEL;
        state.lora.power            = DEFAULT_LORA_POWER;
        fmt_hex_bytes(state.aes.key, DEFAULT_AES_KEY);
        state.comm.ack_required     = DEFAULT_COMM_ACK_REQUIRED;
        state.comm.retry_count      = DEFAULT_COMM_RETRY_COUNT;
        state.comm.retry_timeout    = DEFAULT_COMM_RETRY_TIMEOUT;
    }
    pid_main = thread_getpid();

    gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_BOTH, btn_cb, NULL);
    if (gpio_read(BTN0_PIN) == 0) {
        enter_configuration_mode();
    }

    gpio_init(LED0_PIN, GPIO_OUT);
    gpio_init(LED1_PIN, GPIO_OUT);
    gpio_init(LED2_PIN, GPIO_OUT);
    LED0_ON;
    LED1_OFF;
    LED2_OFF;
    // setup hwrng
    hwrng_init();
    // setup AES
    aes_init();
    aes_sync_set_encrypt_key(&aes, state.aes.key, AES_KEY_128);
    // setup LoRa
    packet_consumer = *stdio_write;
    if (lora_init(&(state.lora), *from_lora) != 0) { return 1; }
    // put radio in listen mode
    lora_listen();
    LED0_OFF;

    uint8_t *start;
    size_t n, buffer_free;
    // read from serial
    serial_buffer_count = 0;
    while (loop) {
        if (serial_buffer_count == sizeof(serial_buffer)) { // buffer full
            to_lora((char *)serial_buffer, serial_buffer_count);
            serial_buffer_count = 0;
        }
        start = serial_buffer + serial_buffer_count;
        buffer_free = sizeof(serial_buffer) - serial_buffer_count;
        n = stdio_read((void *)start, buffer_free);
        serial_buffer_count += n;
        if (n > 0 && n < buffer_free) { // no pending chars on stdio
            to_lora((char *)serial_buffer, serial_buffer_count);
            serial_buffer_count = 0;
        }
    }
    enter_configuration_mode();
    return 0;
}
