int serial_init(forward_data_cb_t *forwarder);
int serial_write(char *buffer, size_t len);
void serial_rx_cb(void *arg, unsigned char data);
