#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Read `count` 512-byte sectors starting at `lba` into `buf`.
   Returns 0 on success, -1 on error. */
int ata_read(uint32_t lba, uint8_t count, void *buf);

/* Write `count` 512-byte sectors starting at `lba` from `buf`.
   Returns 0 on success, -1 on error. */
int ata_write(uint32_t lba, uint8_t count, const void *buf);

/* Detect and initialise the primary master ATA drive.
   Returns 1 if a drive is present, 0 otherwise. */
int ata_init(void);

#define ATA_SECTOR_SIZE 512

#endif /* ATA_H */
