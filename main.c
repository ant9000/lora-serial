#include <string.h>
#include "board.h"
#include "xtimer.h"

#include "common.h"
#include "aes.h"
#include "periph/hwrng.h"

struct aes_sync_device aes;
static uint8_t aes_key[16] = {
    0xce, 0x6e, 0x3f, 0xf5, 0x09, 0xc0, 0xee, 0x81,
    0xd7, 0x9b, 0xd8, 0x5c, 0x4b, 0x5e, 0x79, 0x0d
};
#define AES_BUFFER_LEN (MAX_PACKET_LEN + 16 + 12 + 16)
static uint8_t aes_input[AES_BUFFER_LEN], aes_output[AES_BUFFER_LEN];

#define DEBUG 0
#if DEBUG
static char hexdump_buffer[64];
static void debug(char *msg) { serial_write(msg, strlen(msg)); }
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
        serial_write(hexdump_buffer, j*3 + 3);
        memset(hexdump_buffer, ' ', 63);
        hexdump_buffer[63] = 0;
    }
}
#endif

void to_serial(char *buffer, size_t len)
{
    LED0_ON;
    int n = len - 12 - 16;
    if ((n > 0) && (n <= MAX_PACKET_LEN + 16)) {
#if DEBUG
        hexdump("RECEIVED PACKET:\r\n", buffer, len);
#endif
        uint8_t *aes_input = (uint8_t *)buffer;
        uint8_t *nonce = aes_input + n;
        uint8_t *tag = nonce + 12;
        uint8_t verify[16];
        aes_sync_gcm_crypt_and_tag(&aes, AES_DECRYPT, aes_input, aes_output, (size_t)n, nonce, 12, NULL, 0, verify, 16);
        if (memcmp(tag, verify, 16) == 0) {
#if DEBUG
            hexdump("AUTHENTICATED PADDED CONTENT:\r\n",(char *)aes_output, n);
#endif
            // remove padding
            char c = aes_output[n-1];
            if (c <= 16) {
                n -= c;
                serial_write((char *)aes_output, n);
            } else {
                // packet with invalid padding - discard
#if DEBUG
            debug("INVALID PADDING\r\n");
#endif
            }
        } else {
            // unauthenticated packet - discard
#if DEBUG
            debug("PACKET NOT AUTHENTICATED\r\n");
#endif
        }
    } else {
        // invalid length - discard
#if DEBUG
            hexdump("PACKET HAS INVALID LENGTH\r\n", (char *)&len, 4);
#endif
    }
    LED0_OFF;
}

void to_lora(char *buffer, size_t len)
{
    LED1_ON;
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
#endif
    aes_sync_gcm_crypt_and_tag(&aes, AES_ENCRYPT, aes_input, aes_output, len, nonce, 12, NULL, 0, tag, 16);
    memcpy(aes_output + len, nonce, 12);
    len += 12;
    memcpy(aes_output + len, tag, 16);
    len += 16;
#if DEBUG
    hexdump("ENCRYPTED PACKET:\r\n", (char *)aes_output, len);
#endif
    lora_write((char *)aes_output, len);
    LED1_OFF;
}

int main(void)
{
    LED0_ON;
    LED1_ON;
    // setup hwrng
    hwrng_init();
    // setup AES
    aes_init();
    aes_sync_set_encrypt_key(&aes, aes_key, AES_KEY_128);
    // setup LoRa
    if (lora_init(*to_serial) != 0) { return 1; }
    // setup serial
    if (serial_init(*to_lora) != 0) { return 1; }
    // put radio in listen mode
    lora_listen();
    LED0_OFF;
    LED1_OFF;
    // loop forever
    while (1) { xtimer_sleep(1); }
    return 0;
}
