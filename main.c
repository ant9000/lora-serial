#include "board.h"
#include "xtimer.h"

#include "common.h"

int main(void)
{
    LED0_ON;
    // setup LoRa
    if (lora_init(*serial_write) != 0) { return 1; }
    // setup serial
    LED1_ON;
    if (serial_init(*lora_send) != 0) { return 1; }
    // put radio in listen mode
    lora_listen();
    LED0_OFF;
    LED1_OFF;
    // loop forever
    while (1) { xtimer_sleep(1); }
    return 0;
}
