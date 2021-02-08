#include <string.h>

#include "periph/hwrng.h"
#include "periph/pm.h"

#include "thread.h"
#include "fmt.h"
#include "shell.h"

#include "common.h"

static int cmd_lora(int argc, char **argv)
{
    char *usage = "usage: lora (bw|sf|cr|ch) [value]";
    (void)argc;
    (void)argv;
    if (((argc != 2) && (argc != 3)) || (strcmp(argv[0], "lora") != 0)) {
        puts(usage);
        return -1;
    }
    if (strcmp(argv[1], "bw") == 0) {
        if (argc == 3) {
            if (strcmp(argv[2], "125") == 0) {
                state.lora.bandwidth = 0;
            } else if (strcmp(argv[2], "250") == 0) {
                state.lora.bandwidth = 1;
            } else if (strcmp(argv[2], "500") == 0) {
                state.lora.bandwidth = 2;
            } else {
                puts("Bandwidth not in 125, 250, 500");
                return -1;
            }
        }
        printf("Bandwidth: %u kHz\n", 125*(1<<state.lora.bandwidth));
    } else if (strcmp(argv[1], "sf") == 0) {
        if (argc == 3) {
            int value = atoi(argv[2]);
            if ((value >= 6) && (value <= 12)) {
                state.lora.spreading_factor = (uint8_t)value;
            } else {
                puts("Spreading factor not in 6-12");
                return -1;
            }
        }
        printf("Spreading factor: %u\n", state.lora.spreading_factor);
    } else if (strcmp(argv[1], "cr") == 0) {
        if (argc == 3) {
            int value = atoi(argv[2]);
            if ((value >= 5) && (value <= 8)) {
                state.lora.coderate = (uint8_t)value-4;
            } else {
                puts("Coderate not in 5-8");
                return -1;
            }
        }
        printf("Coderate: 4/%u\n", state.lora.coderate+4);
    } else if (strcmp(argv[1], "ch") == 0) {
        if (argc == 3) {
            uint32_t value = atoll(argv[2]);
            state.lora.channel = value;
        }
        printf("Channel: %lu\n", state.lora.channel);
    } else {
        puts(usage);
        return -1;
    }
    return 0;
}


static int cmd_aes(int argc, char **argv)
{
    char *usage = "usage: aes key [generate|value]";
    (void)argc;
    (void)argv;
    if (((argc != 2) && (argc != 3)) || (strcmp(argv[0], "aes") != 0)) {
        puts(usage);
        return -1;
    }
    if (argc == 3) {
        if (strcmp(argv[2], "generate") == 0) {
            hwrng_init();
            hwrng_read(state.aes.key, 16);
        } else if (strlen(argv[2]) == 32) {
            fmt_hex_bytes(state.aes.key, argv[2]);
        } else {
            puts("Key value should be a string of 32 hex digits.");
            return -1;
        }
    }
    char hex[33];
    fmt_bytes_hex(hex, state.aes.key, 16);
    hex[32] = 0;
    printf("Key: %s\n", hex);
    return 0;
}


static int cmd_show(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    puts("LORA");
    printf("  Bandwidth:        %u kHz\n", 125*(1<<state.lora.bandwidth));
    printf("  Spreading factor: %u\n",  state.lora.spreading_factor);
    printf("  Coding rate:      4/%u\n",  state.lora.coderate+4);
    printf("  Channel:          %lu\n", state.lora.channel);
    puts("AES");
    char hex[33];
    fmt_bytes_hex(hex, state.aes.key, 16);
    hex[32] = 0;
    printf("  Key: %s\n", hex);
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
