#include <inttypes.h>
#include "xtimer.h"

typedef void (forward_data_cb_t)(char *buffer, size_t len);
#define MAX_PACKET_LEN 80

#include "lora.h"
