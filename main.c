#include <string.h>
#include "fmt.h"
#include "od.h"
#include "stdio_base.h"
#include "board.h"
#include "mutex.h"
#include "thread.h"

#include "periph/hwrng.h"
#include "periph/gpio.h"

#include "common.h"
#include "configuration.h"
#include "aes.h"

serialized_state_t state;

struct aes_sync_device aes;
static uint8_t aes_input[MAX_PACKET_LEN], aes_output[MAX_PACKET_LEN];

mutex_t lora_write_lock = MUTEX_INIT;
mutex_t serial_write_lock = MUTEX_INIT;

static uint8_t serial_buffer[63];
static size_t serial_buffer_count;
static uint8_t last_sent = 0, last_received = 0;
static kernel_pid_t pid_main;
void to_lora(char *buffer, size_t len, header_t *header);

#define ENABLE_DEBUG 0
#include "debug.h"
#define HEXDUMP(msg, buffer, len) if (ENABLE_DEBUG) { puts(msg); od_hex_dump((char *)buffer, len, 0); }

static void rx_error_cb(void *arg) { (void)arg; LED2_OFF; }
static ztimer_t rx_error_timeout = { .callback = rx_error_cb };
void from_lora(char *buffer, size_t len)
{
    mutex_lock(&serial_write_lock);
    LED0_ON;
    HEXDUMP("RECEIVED PACKET:", buffer, len);
    int n = len - 12 - 16 - sizeof(header_t);
    bool rx_error = 1;
    if ((n > 0) && (n <= MAX_PAYLOAD_LEN + 16)) {
        uint8_t *aes_input = (uint8_t *)buffer;
        HEXDUMP("RECEIVED PAYLOAD:", buffer, n);
        uint8_t *nonce = aes_input + n;
        HEXDUMP("RECEIVED NONCE:", nonce, 12);
        uint8_t *tag = nonce + 12;
        HEXDUMP("RECEIVED TAG:", tag, 16);
        header_t *header = (header_t *)(tag + 16);
        HEXDUMP("RECEIVED HEADER:", header, sizeof(header_t));
        uint8_t verify[16];
        aes_sync_gcm_crypt_and_tag(&aes, AES_DECRYPT, aes_input, aes_output, (size_t)n, nonce, 12, (uint8_t *)header, sizeof(header_t), verify, 16);
        if (memcmp(tag, verify, 16) == 0) {
            HEXDUMP("AUTHENTICATED PADDED CONTENT:", aes_output, n);
            // remove padding
            char c = aes_output[n-1];
            if (c <= 16) {
                n -= c;
                if (header->ack == 0) {
                    if (header->sequence_no != last_received) {
                        last_received = header->sequence_no;
                        rx_error = 0;
                        stdio_write((char *)aes_output, n);
                    } else {
                        DEBUG_PUTS("DISCARDING DUPLICATE");
                    }
                    if (state.comm.ack_required) {
                        hwrng_read(nonce, 12);
                        header->ack = 1;
                        to_lora((char *)nonce, 12, header);
                    }
                } else {
                    if (header->sequence_no == last_sent) {
                        rx_error = 0;
                        msg_t msg;
                        msg.content.value = 1;
                        msg_send(&msg, pid_main);
                    }
                }
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
        HEXDUMP("PACKET HAS INVALID LENGTH", &len, 4);
    }
    LED0_OFF;
    if (rx_error) {
        LED2_ON;
        ztimer_remove(ZTIMER_MSEC, &rx_error_timeout);
        ztimer_set(ZTIMER_MSEC, &rx_error_timeout, 500);
    }
    mutex_unlock(&serial_write_lock);
}

void to_lora(char *buffer, size_t len, header_t *header)
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
    HEXDUMP("PADDED PACKET:", aes_input, len);
    aes_sync_gcm_crypt_and_tag(&aes, AES_ENCRYPT, aes_input, aes_output, len, nonce, 12, (uint8_t *)header, sizeof(header_t), tag, 16);
    HEXDUMP("NONCE:", nonce, 12);
    memcpy(aes_output + len, nonce, 12);
    len += 12;
    HEXDUMP("TAG:", tag, 16);
    memcpy(aes_output + len, tag, 16);
    len += 16;
    HEXDUMP("HEADER:", header, sizeof(header_t));
    memcpy(aes_output + len, (uint8_t *)header, sizeof(header_t));
    len += sizeof(header_t);
    HEXDUMP("ENCRYPTED PACKET:", aes_output, len);
    lora_write((char *)aes_output, len);
    if ((header->ack == 0) && state.comm.ack_required) {
        // resend packet periodically until it is ack'd or the retry count has exceeded
        msg_t msg;
        msg.content.value = 0;
        size_t retry_count = 0;
        while (((msg.type == MSG_ZTIMER) || (msg.content.value == 0))) {
            if ((state.comm.retry_count > 0) && (retry_count++ >= state.comm.retry_count)) { break; }
            ztimer_msg_receive_timeout(ZTIMER_MSEC, &msg, state.comm.retry_timeout);
            if ((msg.type == MSG_ZTIMER) || (msg.content.value == 0)) { lora_write((char *)aes_output, len); }
        }
    }
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
        state.comm.ack_required     = DEFAULT_COMM_ACK_REQUIRED;
        state.comm.retry_count      = DEFAULT_COMM_RETRY_COUNT;
        state.comm.retry_timeout    = DEFAULT_COMM_RETRY_TIMEOUT;
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

    uint8_t *start;
    size_t n, buffer_free;
    pid_main = thread_getpid();
    // read from serial
    serial_buffer_count = 0;
    header_t header;
    while (loop) {
        if (serial_buffer_count == sizeof(serial_buffer)) { // buffer full
            header.sequence_no = ++last_sent;
            header.ack = 0;
            to_lora((char *)serial_buffer, serial_buffer_count, &header);
            serial_buffer_count = 0;
        }
        start = serial_buffer + serial_buffer_count;
        buffer_free = sizeof(serial_buffer) - serial_buffer_count;
        n = stdio_read((void *)start, buffer_free);
        serial_buffer_count += n;
        if (n > 0 && n < buffer_free) { // no pending chars on stdio
            header.sequence_no = ++last_sent;
            header.ack = 0;
            to_lora((char *)serial_buffer, serial_buffer_count, &header);
            serial_buffer_count = 0;
        }
    }
    enter_configuration_mode();
    return 0;
}
