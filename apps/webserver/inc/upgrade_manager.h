#ifndef UPGRADE_MANAGER_H
#define UPGRADE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    UPGRADE_IDLE,
    UPGRADE_INSTALLING,
    UPGRADE_DONE,
    UPGRADE_ERROR
} UpgradeStatus;

bool upgrade_start(const char *file_path);
void upgrade_get_status(UpgradeStatus *status, char *msg, size_t len);

#endif // UPGRADE_MANAGER_H
