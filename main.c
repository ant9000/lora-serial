#include <string.h>
#include "debug.h"
#include "od.h"
#include "stdio_base.h"
#include "board.h"
#include "mutex.h"

#include "periph/hwrng.h"
#include "periph/gpio.h"

#include "common.h"
#include "configuration.h"
#include "aes.h"


serialized_state_t state;

struct aes_sync_device aes;
static uint8_t aes_key[16] = {
    0xce, 0x6e, 0x3f, 0xf5, 0x09, 0xc0, 0xee, 0x81,
    0xd7, 0x9b, 0xd8, 0x5c, 0x4b, 0x5e, 0x79, 0x0d
};
#define AES_BUFFER_LEN (MAX_PACKET_LEN + 16 + 12 + 16)
static uint8_t aes_input[AES_BUFFER_LEN], aes_output[AES_BUFFER_LEN];

mutex_t lora_write_lock, serial_write_lock;

static uint8_t serial_buffer[31];
static size_t serial_buffer_count;

#define HEXDUMP(msg, buffer, len) if (ENABLE_DEBUG) { puts(msg); od_hex_dump(buffer, len, 0); }

void to_serial(char *buffer, size_t len)
{
    mutex_lock(&serial_write_lock);
    LED0_ON;
    int n = len - 12 - 16;
    if ((n > 0) && (n <= MAX_PACKET_LEN + 16)) {
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

int main(void)
{
    if (load_from_flash (&state, sizeof(state)) < 0) {
        lora_default_config(&(state.lora));
        memcpy(state.aes.key, aes_key, sizeof(aes_key));
    }

    gpio_init(BTN0_PIN, BTN0_MODE);
    if (gpio_read(BTN0_PIN) == 0) {
        enter_configuration_mode();
    }

    LED0_ON;
    LED1_ON;
    // setup hwrng
    hwrng_init();
    // setup AES
    aes_init();
    aes_sync_set_encrypt_key(&aes, state.aes.key, AES_KEY_128);
    // setup LoRa
    if (lora_init(&(state.lora), *to_serial) != 0) { return 1; }
    // put radio in listen mode
    lora_listen();
    LED0_OFF;
    LED1_OFF;

    uint8_t *start;
    size_t n, buffer_free;
    // read from serial
    serial_buffer_count = 0;
    while (1) {
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
    return 0;
}
