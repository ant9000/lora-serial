#include <string.h>
#include "stdio_base.h"
#include "board.h"
#include "mutex.h"

#include "common.h"
#include "aes.h"
#include "periph/hwrng.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef USE_AES
#define USE_AES 1
#endif

#if USE_AES
struct aes_sync_device aes;
static uint8_t aes_key[16] = {
    0xce, 0x6e, 0x3f, 0xf5, 0x09, 0xc0, 0xee, 0x81,
    0xd7, 0x9b, 0xd8, 0x5c, 0x4b, 0x5e, 0x79, 0x0d
};
#define AES_BUFFER_LEN (MAX_PACKET_LEN + 16 + 12 + 16)
static uint8_t aes_input[AES_BUFFER_LEN], aes_output[AES_BUFFER_LEN];
#endif // USE_AES

mutex_t lora_write_lock, serial_write_lock;

static uint8_t serial_buffer[31];
static size_t serial_buffer_count;

#if DEBUG
static char hexdump_buffer[64];
static void debug(char *msg) { stdio_write(msg, strlen(msg)); }
static void hexdump(char *msg, char *buffer, size_t len)
{
    debug(msg);
    memset(hexdump_buffer, ' ', 63);
    hexdump_buffer[63] = 0;
    for(size_t i=0; i<len; i+=8) {
        size_t j;
        for(j=0; (j<8) && (i+j)<len; j++){
            char c = buffer[i+j];
            hexdump_buffer[j*3]     = "0123456789ABCDEF"[(c >> 4) & 0x0F];
            hexdump_buffer[j*3 + 1] = "0123456789ABCDEF"[c & 0x0F];
        }
        j++;
        hexdump_buffer[j*3]     = '\r';
        hexdump_buffer[j*3 + 1] = '\n';
        hexdump_buffer[j*3 + 2] = 0;
        stdio_write(hexdump_buffer, j*3 + 3);
        memset(hexdump_buffer, ' ', 63);
        hexdump_buffer[63] = 0;
    }
}
#endif // DEBUG

void to_serial(char *buffer, size_t len)
{
    mutex_lock(&serial_write_lock);
    LED0_ON;
#if USE_AES
    int n = len - 12 - 16;
    if ((n > 0) && (n <= MAX_PACKET_LEN + 16)) {
#if DEBUG
        hexdump("RECEIVED PACKET:\r\n", buffer, len);
#endif // DEBUG
        uint8_t *aes_input = (uint8_t *)buffer;
        uint8_t *nonce = aes_input + n;
        uint8_t *tag = nonce + 12;
        uint8_t verify[16];
        aes_sync_gcm_crypt_and_tag(&aes, AES_DECRYPT, aes_input, aes_output, (size_t)n, nonce, 12, NULL, 0, verify, 16);
        if (memcmp(tag, verify, 16) == 0) {
#if DEBUG
            hexdump("AUTHENTICATED PADDED CONTENT:\r\n",(char *)aes_output, n);
#endif // DEBUG
            // remove padding
            char c = aes_output[n-1];
            if (c <= 16) {
                n -= c;
                stdio_write((char *)aes_output, n);
            } else {
                // packet with invalid padding - discard
#if DEBUG
            debug("INVALID PADDING\r\n");
#endif // DEBUG
            }
        } else {
            // unauthenticated packet - discard
#if DEBUG
            debug("PACKET NOT AUTHENTICATED\r\n");
#endif // DEBUG
        }
    } else {
        // invalid length - discard
#if DEBUG
            hexdump("PACKET HAS INVALID LENGTH\r\n", (char *)&len, 4);
#endif // DEBUG
    }
#else
    stdio_write((const uint8_t *)buffer, len);
#endif // USE_AES
    LED0_OFF;
    mutex_unlock(&serial_write_lock);
}

void to_lora(char *buffer, size_t len)
{
    mutex_lock(&lora_write_lock);
    LED1_ON;
#if USE_AES
    uint8_t nonce[12];
    uint8_t tag[16];
    hwrng_read(nonce, 12);
    // pad input to a multiple of 128 bits using PKCS#7
    memcpy(aes_input, (uint8_t *)buffer, len);
    char c = 16 - (len % 16);
    memset(aes_input + len, c, c);
    len += c;
#if DEBUG
    hexdump("PADDED PACKET:\r\n", (char *)aes_input, len);
#endif // DEBUG
    aes_sync_gcm_crypt_and_tag(&aes, AES_ENCRYPT, aes_input, aes_output, len, nonce, 12, NULL, 0, tag, 16);
    memcpy(aes_output + len, nonce, 12);
    len += 12;
    memcpy(aes_output + len, tag, 16);
    len += 16;
#if DEBUG
    hexdump("ENCRYPTED PACKET:\r\n", (char *)aes_output, len);
#endif // DEBUG
    lora_write((char *)aes_output, len);
#else
    lora_write(buffer, len);
#endif // USE_AES
puts("SENT");
    LED1_OFF;
    mutex_unlock(&lora_write_lock);
}

int main(void)
{
    LED0_ON;
    LED1_ON;
#if USE_AES
    // setup hwrng
    hwrng_init();
    // setup AES
    aes_init();
    aes_sync_set_encrypt_key(&aes, aes_key, AES_KEY_128);
#endif // USE_AES
    // setup LoRa
    if (lora_init(*to_serial) != 0) { return 1; }
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
