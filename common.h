#include <inttypes.h>

typedef int (forward_data_cb_t)(char *buffer, size_t len);
#define MAX_PACKET_LEN 80

#include "lora.h"
#include "serial.h"
