#include <string.h>
#include "fmt.h"
#include "od.h"
#include "stdio_base.h"
#include "board.h"
#include "mutex.h"

#include "periph/hwrng.h"

#include "common.h"
#include "protocol.h"
#include "aes.h"

extern struct aes_sync_device aes;
extern kernel_pid_t pid_main;

static uint8_t aes_input[MAX_PACKET_LEN], aes_output[MAX_PACKET_LEN];

mutex_t lora_write_lock = MUTEX_INIT;
mutex_t lora_read_lock = MUTEX_INIT;

static uint8_t last_sent = 0, last_received = 0;

#define ENABLE_DEBUG 0
#include "debug.h"
#define HEXDUMP(msg, buffer, len) if (ENABLE_DEBUG) { puts(msg); od_hex_dump((char *)buffer, len, 0); }

void _to_lora(const char *buffer, size_t len, header_t *header);

static void rx_error_cb(void *arg) { (void)arg; LED0_OFF; }
static ztimer_t rx_error_timeout = { .callback = rx_error_cb };
void from_lora(const char *buffer, size_t len)
{
    mutex_lock(&lora_read_lock);
    LED2_ON;
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
                        _to_lora((char *)nonce, 12, header);
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
    LED2_OFF;
    if (rx_error) {
        LED0_ON;
        ztimer_remove(ZTIMER_MSEC, &rx_error_timeout);
        ztimer_set(ZTIMER_MSEC, &rx_error_timeout, 500);
    }
    mutex_unlock(&lora_read_lock);
}

void to_lora(const char *buffer, size_t len)
{
    header_t header;
    header.ack = 0;
    header.sequence_no = ++last_sent;
    _to_lora(buffer, len, &header);
}

void _to_lora(const char *buffer, size_t len, header_t *header)
{
    mutex_lock(&lora_write_lock);
    LED1_ON;
    uint8_t nonce[12];
    uint8_t tag[16];
    header_t h;

    if (header == NULL) {
        h.ack = 0;
        h.sequence_no = ++last_sent;
        header = &h;
    }

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
