#include <inttypes.h>

typedef size_t(forward_data_cb_t)(char *buffer, size_t len);

#include "lora.h"
#include "serial.h"
