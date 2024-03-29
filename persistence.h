#ifndef PERSISTENCE_H
#define PERSISTENCE_H

int load_from_flash (void *buffer, size_t len);
int save_to_flash (const void *buffer, size_t len);
int erase_flash (void);

#endif /* PERSISTENCE_H */
