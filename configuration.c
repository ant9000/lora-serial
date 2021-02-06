#include <string.h>

#include "periph/gpio.h"
#include "periph/pm.h"

#include "luid.h"
#include "thread.h"
#include "fmt.h"
#include "shell.h"

#include "common.h"

static int cmd_lora(int argc, char **argv)
{
    char *usage = "usage: lora (bw|sf|cr|ch) [value]";
    (void)argc;
    (void)argv;
    puts("TODO");
    puts(usage);
    return 0;
}


static int cmd_aes(int argc, char **argv)
{
    char *usage = "usage: aes (key) [generate]";
    (void)argc;
    (void)argv;
    puts("TODO");
    puts(usage);
    return 0;
}


static int cmd_show(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    puts("LORA");
    printf("Bandwidth:        %u\n",  state.lora.bandwidth);
    printf("Spreading factor: %u\n",  state.lora.spreading_factor);
    printf("Coding rate:      %u\n",  state.lora.coderate);
    printf("Channel:          %lu\n", state.lora.channel);
    puts("AES");
    printf("Key: ");
    for(size_t i = 0; i < sizeof(state.aes.key); i++) {
        printf("%02x%s", state.aes.key[i], (i < sizeof(state.aes.key) - 1 ? ":" : ""));
    }
    printf("\n");
    return 0;
}

static int cmd_load(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (load_from_flash(&state, sizeof(state)) < 0) {
        puts("Flash is empty.");
    }
    return 0;
}

static int cmd_erase(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    puts("Erasing stored parameters.");
    if (erase_flash() < 0) {
        puts("Error erasing flash.");

    }
    return 0;
}

static int cmd_save(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (save_to_flash(&state, sizeof(state)) < 0) {
        puts("Error saving to flash.");
    }
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    pm_reboot();
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "lora",   "get/set lora values",  cmd_lora   },
    { "aes",    "get/set aes values",   cmd_aes    },
    { "show",   "show configuration",   cmd_show   },
    { "load",   "load flash contents",  cmd_load   },
    { "erase",  "clear flash contents", cmd_erase  },
    { "save",   "save to flash",        cmd_save   },
    { "reboot", "reboot",               cmd_reboot },
    { NULL, NULL, NULL }
};

static char _stack[THREAD_STACKSIZE_DEFAULT];
void *blink_led(void *arg) {
    (void)arg;

    gpio_init(LED1_PIN, GPIO_OUT);
    while (true) {
        gpio_toggle(LED1_PIN);
        xtimer_sleep(1);
    }
}

void enter_configuration_mode(void) {
    thread_create(_stack, sizeof(_stack), THREAD_PRIORITY_MAIN - 1,
        THREAD_CREATE_WOUT_YIELD | THREAD_CREATE_STACKTEST,
        blink_led, NULL, "led");

    char line_buf[SHELL_DEFAULT_BUFSIZE];

    puts("Entering config mode - use 'help' for enumerating commands.");
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
}
