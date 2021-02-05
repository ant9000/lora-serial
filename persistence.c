#include <string.h>
#include <assert.h>
#include "periph/flashpage.h"

#include "common.h"

#define RWWEE_PAGE 0
#define SIGNATURE "RIOT"
#define SIGNATURE_LEN 4

int load_from_flash (void *buffer, size_t len)
{
    uint8_t page[FLASHPAGE_SIZE];
    assert(len + SIGNATURE_LEN < FLASHPAGE_SIZE);
    flashpage_rwwee_read(RWWEE_PAGE, page);
    if (strncmp((char *)page, SIGNATURE, SIGNATURE_LEN) != 0) { return -1; }
    memcpy((uint8_t *)buffer, page + SIGNATURE_LEN, len);
    return 0;
}

int save_to_flash (const void *buffer, size_t len)
{
    uint8_t page[FLASHPAGE_SIZE];
    assert(len + SIGNATURE_LEN < FLASHPAGE_SIZE);
    flashpage_rwwee_read(RWWEE_PAGE, page);
    // do not rewrite flash unless needed
    if (
        (strncmp((char *)page, SIGNATURE, SIGNATURE_LEN) != 0) ||
        (memcmp(buffer, page + SIGNATURE_LEN, len) != 0)
    ) {
        memcpy(page, SIGNATURE, SIGNATURE_LEN);
        memcpy(page + SIGNATURE_LEN, buffer, len);
        if (flashpage_rwwee_write_and_verify(RWWEE_PAGE, page) != FLASHPAGE_OK) { return -1; }
        return 1;
    }
    return 0;
}

int erase_flash (void)
{
    uint8_t page[FLASHPAGE_SIZE];
    memset(page, 0, FLASHPAGE_SIZE);
    if (flashpage_rwwee_write_and_verify(RWWEE_PAGE, page) != FLASHPAGE_OK) { return -1; }
    return 0;
}
