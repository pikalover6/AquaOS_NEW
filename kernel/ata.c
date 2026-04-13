#include "ata.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

/* Primary ATA bus, master drive (PIO 28-bit LBA) */
#define ATA_DATA        0x1F0
#define ATA_ERR         0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_CMD         0x1F7
#define ATA_ALT_STATUS  0x3F6

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static int drive_present = 0;

static int ata_wait_bsy(void) {
    int timeout = 100000;
    while ((inb(ATA_STATUS) & ATA_SR_BSY) && timeout-- > 0);
    return (timeout > 0) ? 0 : -1;
}

static int ata_wait_drq(void) {
    int timeout = 100000;
    while (!(inb(ATA_STATUS) & ATA_SR_DRQ) && timeout-- > 0) {
        if (inb(ATA_STATUS) & ATA_SR_ERR) return -1;
    }
    return (timeout > 0) ? 0 : -1;
}

int ata_init(void) {
    outb(ATA_DRIVE_HEAD, 0xA0);   /* select master */
    for (int i = 0; i < 15; i++) inb(ATA_ALT_STATUS); /* delay */

    /* Send IDENTIFY */
    outb(ATA_SECTOR_CNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);
    outb(ATA_CMD, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS);
    if (status == 0) return 0;  /* no drive */

    if (ata_wait_bsy() < 0) return 0;

    /* Check for ATAPI */
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) return 0;

    if (ata_wait_drq() < 0) return 0;

    /* Read identity data (discard) */
    for (int i = 0; i < 256; i++) inw(ATA_DATA);

    drive_present = 1;
    return 1;
}

int ata_read(uint32_t lba, uint8_t count, void *buf) {
    if (!drive_present) return -1;
    if (count == 0) return 0;

    if (ata_wait_bsy() < 0) return -1;

    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_ERR,        0x00);
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO,     (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID,    (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,     (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD,        ATA_CMD_READ_SECTORS);

    uint16_t *p = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_bsy() < 0)  return -1;
        if (ata_wait_drq() < 0)  return -1;
        for (int i = 0; i < 256; i++) p[s * 256 + i] = inw(ATA_DATA);
    }
    return 0;
}

int ata_write(uint32_t lba, uint8_t count, const void *buf) {
    if (!drive_present) return -1;
    if (count == 0) return 0;

    if (ata_wait_bsy() < 0) return -1;

    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_ERR,        0x00);
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO,     (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID,    (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,     (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD,        ATA_CMD_WRITE_SECTORS);

    const uint16_t *p = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_bsy() < 0) return -1;
        if (ata_wait_drq() < 0) return -1;
        for (int i = 0; i < 256; i++) outw(ATA_DATA, p[s * 256 + i]);
        outb(ATA_CMD, 0xE7); /* flush cache */
        ata_wait_bsy();
    }
    return 0;
}
