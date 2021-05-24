#include <string.h>
#include "fmt.h"
#include "od.h"
#include "stdio_base.h"
#include "board.h"
#include "mutex.h"

#include "periph/hwrng.h"
#include "periph/gpio.h"

#include "common.h"
#include "configuration.h"
#include "aes.h"

#define XON  0x11
#define XOFF 0x13

serialized_state_t state;

struct aes_sync_device aes;
static uint8_t aes_input[MAX_PACKET_LEN], aes_output[MAX_PACKET_LEN];

mutex_t lora_write_lock, serial_write_lock;

static uint8_t serial_buffer[63];
static size_t serial_buffer_count;

#define ENABLE_DEBUG 0
#include "debug.h"
#define HEXDUMP(msg, buffer, len) if (ENABLE_DEBUG) { puts(msg); od_hex_dump(buffer, len, 0); }

static void discarded_cb(void *arg) { (void)arg; LED2_OFF; }
static ztimer_t discarded_timeout = { .callback = discarded_cb };
void from_lora(char *buffer, size_t len)
{
    mutex_lock(&serial_write_lock);
    LED0_ON;
    int n = len - 12 - 16;
    bool discarded = 1;
    if ((n > 0) && (n <= MAX_PAYLOAD_LEN + 16)) {
        HEXDUMP("RECEIVED PACKET:", buffer, len);
        uint8_t *aes_input = (uint8_t *)buffer;
        uint8_t *nonce = aes_input + n;
        uint8_t *tag = nonce + 12;
        uint8_t verify[16];
        aes_sync_gcm_crypt_and_tag(&aes, AES_DECRYPT, aes_input, aes_output, (size_t)n, nonce, 12, NULL, 0, verify, 16);
        if (memcmp(tag, verify, 16) == 0) {
            HEXDUMP("AUTHENTICATED PADDED CONTENT:",(char *)aes_output, n);
            // remove padding
            char c = aes_output[n-1];
            if (c <= 16) {
                n -= c;
                stdio_write((char *)aes_output, n);
                discarded = 0;
            } else {
                // packet with invalid padding - discard
                DEBUG_PUTS("INVALID PADDING");
            }
        } else {
            // unauthenticated packet - discard
            DEBUG_PUTS("PACKET NOT AUTHENTICATED");
        }
    } else {
        // invalid length - discard
        HEXDUMP("PACKET HAS INVALID LENGTH", (char *)&len, 4);
    }
    LED0_OFF;
    if (discarded) {
        LED2_ON;
        ztimer_remove(ZTIMER_MSEC, &discarded_timeout);
        ztimer_set(ZTIMER_MSEC, &discarded_timeout, 500);
    }
    mutex_unlock(&serial_write_lock);
}

void to_lora(char *buffer, size_t len)
{
    mutex_lock(&lora_write_lock);
    LED1_ON;
    uint8_t nonce[12];
    uint8_t tag[16];
    hwrng_read(nonce, 12);
    // pad input to a multiple of 128 bits using PKCS#7
    memcpy(aes_input, (uint8_t *)buffer, len);
    char c = 16 - (len % 16);
    memset(aes_input + len, c, c);
    len += c;
    HEXDUMP("PADDED PACKET:", (char *)aes_input, len);
    aes_sync_gcm_crypt_and_tag(&aes, AES_ENCRYPT, aes_input, aes_output, len, nonce, 12, NULL, 0, tag, 16);
    memcpy(aes_output + len, nonce, 12);
    len += 12;
    memcpy(aes_output + len, tag, 16);
    len += 16;
    HEXDUMP("ENCRYPTED PACKET:", (char *)aes_output, len);
    lora_write((char *)aes_output, len);
    LED1_OFF;
    mutex_unlock(&lora_write_lock);
}

static bool loop=1;
static void timeout_cb(void *arg)
{
    (void)arg;
    if (loop) {
        loop=0;
        LED1_ON;
        puts("Entering config mode - use 'help' for enumerating commands.");
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
    if (load_from_flash (&state, sizeof(state)) < 0) {
        memset(&state, 0, sizeof(state));
        state.lora.bandwidth        = DEFAULT_LORA_BANDWIDTH;
        state.lora.spreading_factor = DEFAULT_LORA_SPREADING_FACTOR;
        state.lora.coderate         = DEFAULT_LORA_CODERATE;
        state.lora.channel          = DEFAULT_LORA_CHANNEL;
        state.lora.power            = DEFAULT_LORA_POWER;
        fmt_hex_bytes(state.aes.key, DEFAULT_AES_KEY);
    }

    gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_BOTH, btn_cb, NULL);
    if (gpio_read(BTN0_PIN) == 0) {
        puts("Entering config mode - use 'help' for enumerating commands.");
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
    if (lora_init(&(state.lora), *from_lora) != 0) { return 1; }
    // put radio in listen mode
    lora_listen();
    LED0_OFF;

    uint8_t *start, flow;
    size_t n, buffer_free;
    // read from serial
    serial_buffer_count = 0;
    while (loop) {
        if (serial_buffer_count == sizeof(serial_buffer)) { // buffer full
            flow = XOFF;
            stdio_write(&flow, 1);
            to_lora((char *)serial_buffer, serial_buffer_count);
            serial_buffer_count = 0;
            flow = XON;
            stdio_write(&flow, 1);
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
