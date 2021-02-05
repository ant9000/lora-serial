#include <inttypes.h>
#include "xtimer.h"

#define MAX_PACKET_LEN 80

#include "lora.h"
#include "persistence.h"

typedef struct {
    uint8_t key[16];
} aes_state_t;

typedef struct {
    lora_state_t lora;
    aes_state_t  aes;
} serialized_state_t;

extern serialized_state_t state;
