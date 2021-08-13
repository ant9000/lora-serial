#ifndef LORA_H
#define LORA_H

typedef struct {
    uint8_t  bandwidth;
    uint8_t  spreading_factor;
    uint8_t  coderate;
    uint32_t channel;
    int16_t  power;
} lora_state_t;

typedef void (forward_data_cb_t)(char *buffer, size_t len);

int lora_init(const lora_state_t *state, forward_data_cb_t *forwarder);
int lora_write(char *msg, size_t len);
void lora_listen(void);

#endif /* LORA_H */
