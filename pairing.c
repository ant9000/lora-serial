#include "fmt.h"
#include "periph/pm.h"

#include "board.h"
#include "pairing.h"
#include "protocol.h"
#include "aes.h"

extern struct aes_sync_device aes;
extern kernel_pid_t pid_main;
extern consume_data_cb_t *packet_consumer;

static ztimer_t timeout;
static void timeout_cb(void *arg)
{
    (void)arg;
    gpio_toggle(LED2_PIN);
    ztimer_set(ZTIMER_MSEC, &timeout, 1000);
}

ssize_t packet_received(const void *buffer, size_t len)
{
    (void)buffer;
    (void)len;
    // TODO
    return 0;
}

void enter_pairing_mode(void)
{
    gpio_init(LED2_PIN, GPIO_OUT);
    LED2_ON;

    timeout.callback = timeout_cb;
    ztimer_set(ZTIMER_MSEC, &timeout, 1000);

    puts("Entering pairing mode.");

    state.lora.power = 0;
    fmt_hex_bytes(state.aes.key, DEFAULT_AES_KEY);
    packet_consumer = *packet_received;

    aes_sync_set_encrypt_key(&aes, state.aes.key, AES_KEY_128);
    if (lora_init(&(state.lora), *from_lora) != 0) { return; }
    lora_listen();

    // TODO:
    //  pose as client and listen for 500ms for a valid radio packet
    //  if nothing is received, switch to server mode
    //  create a random key and send it repeatedly until it gets ack'd
    //  if the key is ack'd, save it to flash and reboot
    //  if no ack is received in 5s, restore defaults (erasing flash) and reboot

    ztimer_sleep(ZTIMER_MSEC, 15000);
    ztimer_remove(ZTIMER_MSEC, &timeout);
    LED2_OFF;

    puts("Exiting pairing mode.");
    pm_reboot();
}
