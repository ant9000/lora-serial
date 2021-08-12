#include <inttypes.h>
#include "xtimer.h"
#include "net/lora.h"
#include "sx127x.h"

#include "lora.h"
#include "persistence.h"

typedef struct {
    uint8_t sequence_no;
    bool ack;
} header_t;

#define MAX_PAYLOAD_LEN 80
/* AES-GCM requires padding of up to 16 bytes, plus nonce, plus tag, plus header */
#define MAX_PACKET_LEN (MAX_PAYLOAD_LEN + 16 + 12 + 16 + sizeof(header_t))

#define DEFAULT_LORA_BANDWIDTH        LORA_BW_500_KHZ
#define DEFAULT_LORA_SPREADING_FACTOR LORA_SF7
#define DEFAULT_LORA_CODERATE         LORA_CR_4_5
#define DEFAULT_LORA_CHANNEL          SX127X_CHANNEL_DEFAULT
#define DEFAULT_LORA_POWER            SX127X_RADIO_TX_POWER
#define DEFAULT_AES_KEY               "c165dac2f95a4444b52dc92a36e2bc05"
/* ack required provides more reliable transport at the expense of speed - think TCP vs UDP */
/* if an ack is not received within the timeout, resend for the specified number of times   */
#define DEFAULT_COMM_ACK_REQUIRED     1
#define DEFAULT_COMM_RETRY_COUNT      5    /* use 0 to retry forever */
#define DEFAULT_COMM_RETRY_TIMEOUT    100  /* value is in ms */

typedef struct {
    uint8_t key[16];
} aes_state_t;

typedef struct {
    bool ack_required;
    uint8_t retry_count;
    uint16_t retry_timeout;
} comm_state_t;

typedef struct {
    lora_state_t lora;
    aes_state_t  aes;
    comm_state_t comm;
} serialized_state_t;

extern serialized_state_t state;
