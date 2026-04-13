#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stddef.h>

/* ---- Public types ---------------------------------------------------- */

/* Maximum path length */
#define FAT_PATH_MAX 256
/* Maximum filename length (8.3) */
#define FAT_NAME_MAX 13

typedef struct {
    char     name[FAT_NAME_MAX];   /* null-terminated 8.3 name */
    uint8_t  attr;                 /* FAT attribute byte */
    uint32_t size;                 /* file size in bytes */
    uint16_t cluster;              /* starting cluster */
} fat_dirent_t;

#define FAT_ATTR_READONLY  0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLID     0x08
#define FAT_ATTR_DIR       0x10
#define FAT_ATTR_ARCHIVE   0x20

/* ---- Filesystem init ------------------------------------------------- */

/* Mount the FAT16 filesystem on the primary ATA drive.
   Returns 0 on success, -1 if no drive or unrecognised filesystem. */
int fat_init(void);

/* Returns 1 if filesystem is mounted and available. */
int fat_available(void);

/* ---- Directory operations -------------------------------------------- */

/* List directory at path. Calls callback for each entry (excluding . and ..).
   Returns 0 on success, -1 on error. */
int fat_readdir(const char *path,
                void (*cb)(const fat_dirent_t *ent, void *arg), void *arg);

/* Change working directory. Returns 0 on success, -1 on error. */
int fat_chdir(const char *path);

/* Get current working directory into buf (max bufsz bytes). */
void fat_getcwd(char *buf, size_t bufsz);

/* Create a directory. Returns 0 on success, -1 on error. */
int fat_mkdir(const char *path);

/* Delete an empty directory. Returns 0 on success. */
int fat_rmdir(const char *path);

/* ---- File operations ------------------------------------------------- */

/* Read entire file into buf (max bufsz bytes). Returns bytes read or -1. */
int fat_read(const char *path, char *buf, size_t bufsz);

/* Write (truncate + write) data to file. Returns 0 on success, -1 on error. */
int fat_write(const char *path, const char *data, size_t len);

/* Append data to file (creates if not exists). Returns 0 on success. */
int fat_append(const char *path, const char *data, size_t len);

/* Create an empty file. Returns 0 on success, -1 on error. */
int fat_create(const char *path);

/* Delete a file. Returns 0 on success, -1 on error. */
int fat_unlink(const char *path);

#endif /* FAT_H */
