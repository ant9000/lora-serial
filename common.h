#include <inttypes.h>
#include "xtimer.h"
#include "net/lora.h"
#include "sx127x.h"

#include "lora.h"
#include "persistence.h"

#define MAX_PAYLOAD_LEN 80
/* AES-GCM requires padding of up to 16 bytes, plus nonce, plus tag */
#define MAX_PACKET_LEN (MAX_PAYLOAD_LEN + 16 + 12 + 16)

#define DEFAULT_LORA_BANDWIDTH        LORA_BW_500_KHZ
#define DEFAULT_LORA_SPREADING_FACTOR LORA_SF7
#define DEFAULT_LORA_CODERATE         LORA_CR_4_5
#define DEFAULT_LORA_CHANNEL          SX127X_CHANNEL_DEFAULT
#define DEFAULT_AES_KEY               "c165dac2f95a4444b52dc92a36e2bc05"

typedef struct {
    uint8_t key[16];
} aes_state_t;

typedef struct {
    lora_state_t lora;
    aes_state_t  aes;
} serialized_state_t;

extern serialized_state_t state;
