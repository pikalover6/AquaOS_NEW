#include "fat.h"
#include "ata.h"
#include "string.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

/* ---- FAT16 on-disk structures --------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat16_bpb_t;

typedef struct __attribute__((packed)) {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t cluster_hi;   /* always 0 for FAT16 */
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_lo;
    uint32_t file_size;
} fat16_rawent_t;

/* ---- Module state --------------------------------------------------- */
static int           fs_ok         = 0;
static uint16_t      bytes_per_sec = 512;
static uint8_t       spc           = 0;    /* sectors per cluster */
static uint32_t      fat_start     = 0;    /* LBA of FAT */
static uint32_t      root_start    = 0;    /* LBA of root directory */
static uint32_t      data_start    = 0;    /* LBA of first data cluster */
static uint16_t      root_entries  = 0;
/* (fat_buf reserved for future cached-FAT use) */

/* Current working directory cluster (0 = root) */
static uint16_t cwd_cluster = 0;
/* CWD path string */
static char cwd_path[FAT_PATH_MAX] = "0:\\";

/* ---- Helpers -------------------------------------------------------- */
static int read_sector(uint32_t lba, void *buf) {
    return ata_read(lba, 1, buf);
}

static int write_sector(uint32_t lba, const void *buf) {
    return ata_write(lba, 1, (void*)buf);
}

static uint32_t cluster_to_lba(uint16_t cluster) {
    return data_start + (uint32_t)(cluster - 2) * spc;
}

static uint16_t fat_entry(uint16_t cluster) {
    /* Each FAT16 entry is 2 bytes; 256 entries per 512-byte sector */
    uint32_t fat_sector = fat_start + cluster / 256;
    uint16_t buf[256];
    if (read_sector(fat_sector, buf) < 0) return 0xFFFF;
    return buf[cluster % 256];
}

static int fat_set_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_sector = fat_start + cluster / 256;
    uint16_t buf[256];
    if (read_sector(fat_sector, buf) < 0) return -1;
    buf[cluster % 256] = value;
    if (write_sector(fat_sector, buf) < 0) return -1;
    return 0;
}

/* Find a free cluster (returns 0 on failure) */
static uint16_t alloc_cluster(void) {
    /* Read FAT sectors and look for 0x0000 entries */
    uint32_t total_fat_sectors = (uint32_t)(root_start - fat_start);
    for (uint32_t s = 0; s < total_fat_sectors; s++) {
        uint16_t buf[256];
        if (read_sector(fat_start + s, buf) < 0) return 0;
        for (int i = 0; i < 256; i++) {
            uint16_t cl = (uint16_t)(s * 256 + i);
            if (cl < 2) continue;
            if (buf[i] == 0x0000) {
                buf[i] = 0xFFFF; /* mark end of chain */
                write_sector(fat_start + s, buf);
                /* zero the cluster data */
                static uint8_t zero[512];
                memset(zero, 0, 512);
                uint32_t lba = cluster_to_lba(cl);
                for (uint8_t k = 0; k < spc; k++)
                    write_sector(lba + k, zero);
                return cl;
            }
        }
    }
    return 0;
}

/* ---- 8.3 name helpers ---------------------------------------------- */
static void make_83(const char *name, char out[11]) {
    memset(out, ' ', 11);
    int dot = -1;
    int len = (int)strlen(name);
    for (int i = 0; i < len; i++) if (name[i] == '.') dot = i;
    int base_len = dot >= 0 ? dot : len;
    if (base_len > 8) base_len = 8;
    for (int i = 0; i < base_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = c;
    }
    if (dot >= 0) {
        int ext_len = len - dot - 1;
        if (ext_len > 3) ext_len = 3;
        for (int i = 0; i < ext_len; i++) {
            char c = name[dot + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + i] = c;
        }
    }
}

static void parse_83(const fat16_rawent_t *e, char out[FAT_NAME_MAX]) {
    int i = 0, j = 0;
    /* base name */
    while (j < 8 && e->name[j] != ' ') out[i++] = e->name[j++];
    /* extension */
    if (e->ext[0] != ' ') {
        out[i++] = '.';
        for (j = 0; j < 3 && e->ext[j] != ' '; j++)
            out[i++] = e->ext[j];
    }
    out[i] = '\0';
}

/* ---- Directory iteration ------------------------------------------- */
typedef struct {
    uint32_t sector;   /* current sector LBA */
    int      index;    /* entry index within sector (0-15) */
    int      is_root;
    uint32_t root_end;
    uint16_t cluster;
    uint8_t  cluster_sector;
} dir_iter_t;

static void dir_iter_root(dir_iter_t *it) {
    it->sector       = root_start;
    it->index        = 0;
    it->is_root      = 1;
    it->root_end     = root_start + (root_entries * 32 + 511) / 512;
    it->cluster      = 0;
    it->cluster_sector = 0;
}

static void dir_iter_cluster(dir_iter_t *it, uint16_t cluster) {
    it->sector       = cluster_to_lba(cluster);
    it->index        = 0;
    it->is_root      = 0;
    it->root_end     = 0;
    it->cluster      = cluster;
    it->cluster_sector = 0;
}

/* Returns 1 if entry read into *e, 0 if end of directory, -1 on error */
static int dir_next(dir_iter_t *it, fat16_rawent_t *e, uint32_t *out_lba, int *out_idx) {
    uint8_t buf[512];
    while (1) {
        /* Check if we need a new sector */
        if (it->index == 16) {
            it->index = 0;
            it->sector++;
            if (!it->is_root) {
                it->cluster_sector++;
                if (it->cluster_sector >= spc) {
                    uint16_t next = fat_entry(it->cluster);
                    if (next >= 0xFFF8) return 0; /* end of chain */
                    it->cluster        = next;
                    it->cluster_sector = 0;
                    it->sector = cluster_to_lba(it->cluster);
                }
            }
        }
        if (it->is_root && it->sector >= it->root_end) return 0;

        if (read_sector(it->sector, buf) < 0) return -1;
        fat16_rawent_t *ep = (fat16_rawent_t *)buf + it->index;

        if ((uint8_t)ep->name[0] == 0x00) return 0;  /* end of directory */
        if ((uint8_t)ep->name[0] == 0xE5) { it->index++; continue; } /* deleted */
        if (ep->attr & (FAT_ATTR_VOLID)) { it->index++; continue; }  /* volume id */

        *e       = *ep;
        if (out_lba) *out_lba = it->sector;
        if (out_idx) *out_idx = it->index;
        it->index++;
        return 1;
    }
}

/* Find entry by name in directory at `dir_cluster` (0=root).
   Returns 1 found, 0 not found, -1 error. */
static int dir_find(uint16_t dir_cluster, const char *name,
                    fat16_rawent_t *out_e, uint32_t *out_lba, int *out_idx) {
    char target[11];
    make_83(name, target);

    dir_iter_t it;
    if (dir_cluster == 0) dir_iter_root(&it);
    else                  dir_iter_cluster(&it, dir_cluster);

    fat16_rawent_t e;
    uint32_t lba; int idx;
    int r;
    while ((r = dir_next(&it, &e, &lba, &idx)) > 0) {
        if (memcmp(e.name, target, 8) == 0 && memcmp(e.ext, target + 8, 3) == 0) {
            if (out_e)   *out_e  = e;
            if (out_lba) *out_lba = lba;
            if (out_idx) *out_idx = idx;
            return 1;
        }
    }
    return (r < 0) ? -1 : 0;
}

/* Resolve a path string to a cluster (0 = root).
   Returns cluster, or 0xFFFF on error. */
static uint16_t resolve_path(const char *path) {
    /* Strip drive prefix "0:\" or "0:/" */
    if (path[0] != '\0' && path[1] == ':') path += 2;
    if (*path == '\\' || *path == '/') {
        path++;
        /* absolute from root */
        if (*path == '\0') return 0; /* root itself */
        uint16_t cur = 0;
        char part[FAT_NAME_MAX];
        while (*path) {
            int i = 0;
            while (*path && *path != '\\' && *path != '/')
                part[i++] = *path++;
            part[i] = '\0';
            if (*path) path++;
            if (i == 0) continue;
            fat16_rawent_t e;
            int r = dir_find(cur, part, &e, 0, 0);
            if (r <= 0) return 0xFFFF;
            if (!(e.attr & FAT_ATTR_DIR)) return 0xFFFF;
            cur = e.cluster_lo;
        }
        return cur;
    }
    /* Relative path from cwd */
    uint16_t cur = cwd_cluster;
    if (*path == '\0') return cur;
    char part[FAT_NAME_MAX];
    while (*path) {
        int i = 0;
        while (*path && *path != '\\' && *path != '/')
            part[i++] = *path++;
        part[i] = '\0';
        if (*path) path++;
        if (i == 0) continue;
        if (strcmp(part, ".") == 0) continue;
        if (strcmp(part, "..") == 0) { cur = 0; continue; } /* simplified */
        fat16_rawent_t e;
        int r = dir_find(cur, part, &e, 0, 0);
        if (r <= 0) return 0xFFFF;
        if (!(e.attr & FAT_ATTR_DIR)) return 0xFFFF;
        cur = e.cluster_lo;
    }
    return cur;
}

/* Add a directory entry in dir `dir_cl`. Returns 0 on success. */
static int dir_add_entry(uint16_t dir_cl, const fat16_rawent_t *e) {
    dir_iter_t it;
    uint8_t buf[512];
    if (dir_cl == 0) dir_iter_root(&it);
    else             dir_iter_cluster(&it, dir_cl);

    fat16_rawent_t tmp;
    uint32_t lba; int idx;
    int r;
    /* Scan for a free slot */
    while ((r = dir_next(&it, &tmp, &lba, &idx)) > 0);
    /* After the loop, it.sector / it.index point past the last valid entry */
    /* We re-scan for deleted (0xE5) or null (0x00) entries */
    if (dir_cl == 0) dir_iter_root(&it);
    else             dir_iter_cluster(&it, dir_cl);

    while (1) {
        if (it.index == 16) {
            it.index = 0;
            it.sector++;
            if (!it.is_root) {
                it.cluster_sector++;
                if (it.cluster_sector >= spc) {
                    uint16_t next = fat_entry(it.cluster);
                    if (next >= 0xFFF8) {
                        /* Extend directory with new cluster */
                        uint16_t nc = alloc_cluster();
                        if (!nc) return -1;
                        fat_set_entry(it.cluster, nc);
                        it.cluster        = nc;
                        it.cluster_sector = 0;
                        it.sector = cluster_to_lba(nc);
                    } else {
                        it.cluster        = next;
                        it.cluster_sector = 0;
                        it.sector = cluster_to_lba(it.cluster);
                    }
                }
            }
        }
        if (it.is_root && it.sector >= it.root_end) return -1; /* root full */

        if (read_sector(it.sector, buf) < 0) return -1;
        fat16_rawent_t *ep = (fat16_rawent_t *)buf + it.index;

        if ((uint8_t)ep->name[0] == 0x00 || (uint8_t)ep->name[0] == 0xE5) {
            *ep = *e;
            return write_sector(it.sector, buf);
        }
        it.index++;
    }
}

/* ---- Public API ----------------------------------------------------- */

int fat_init(void) {
    if (!ata_init()) return -1;

    uint8_t buf[512];
    if (read_sector(0, buf) < 0) return -1;

    fat16_bpb_t *bpb = (fat16_bpb_t *)buf;
    if (bpb->bytes_per_sector != 512) return -1;
    if (memcmp(bpb->fs_type, "FAT16   ", 8) != 0 &&
        memcmp(bpb->fs_type, "FAT12   ", 8) != 0 &&
        memcmp(bpb->fs_type, "FAT     ", 8) != 0) {
        /* Try to detect by other means — just proceed */
    }

    bytes_per_sec = bpb->bytes_per_sector;
    spc           = bpb->sectors_per_cluster;
    fat_start     = bpb->reserved_sectors;
    root_start    = fat_start + (uint32_t)bpb->num_fats * bpb->fat_size_16;
    root_entries  = bpb->root_entry_count;
    data_start    = root_start + (root_entries * 32 + 511) / 512;

    fs_ok         = 1;
    cwd_cluster   = 0;
    strcpy(cwd_path, "0:\\");
    return 0;
}

int fat_available(void) { return fs_ok; }

int fat_readdir(const char *path,
                void (*cb)(const fat_dirent_t *ent, void *arg), void *arg) {
    if (!fs_ok) return -1;
    uint16_t cl = resolve_path(path);
    if (cl == 0xFFFF) return -1;

    dir_iter_t it;
    if (cl == 0) dir_iter_root(&it);
    else         dir_iter_cluster(&it, cl);

    fat16_rawent_t e;
    int r;
    while ((r = dir_next(&it, &e, 0, 0)) > 0) {
        if (e.name[0] == '.') continue;
        fat_dirent_t de;
        parse_83(&e, de.name);
        de.attr    = e.attr;
        de.size    = e.file_size;
        de.cluster = e.cluster_lo;
        cb(&de, arg);
    }
    return (r < 0) ? -1 : 0;
}

int fat_chdir(const char *path) {
    if (!fs_ok) return -1;
    /* Handle "0:\" → root */
    const char *p = path;
    if (p[0] != '\0' && p[1] == ':') p += 2;
    if (*p == '\\' || *p == '/') {
        if (*(p + 1) == '\0') {
            cwd_cluster = 0;
            strcpy(cwd_path, "0:\\");
            return 0;
        }
    }
    uint16_t cl = resolve_path(path);
    if (cl == 0xFFFF) return -1;
    cwd_cluster = cl;
    /* Update path string */
    if (path[0] != '\0' && path[1] == ':') {
        strncpy(cwd_path, path, FAT_PATH_MAX - 1);
    } else {
        /* Append to current */
        size_t cur_len = strlen(cwd_path);
        if (cur_len > 0 && cwd_path[cur_len - 1] != '\\')
            strncat(cwd_path, "\\", FAT_PATH_MAX - cur_len - 1);
        strncat(cwd_path, path, FAT_PATH_MAX - strlen(cwd_path) - 1);
    }
    return 0;
}

void fat_getcwd(char *buf, size_t bufsz) {
    strncpy(buf, cwd_path, bufsz - 1);
    buf[bufsz - 1] = '\0';
}

/* ---- File read ------------------------------------------------------- */
int fat_read(const char *path, char *buf, size_t bufsz) {
    if (!fs_ok) return -1;

    /* Split path into dir and filename */
    char dir_path[FAT_PATH_MAX], fname[FAT_NAME_MAX];
    const char *slash = 0;
    const char *tmp = path;
    while (*tmp) { if (*tmp == '\\' || *tmp == '/') slash = tmp; tmp++; }
    if (slash) {
        size_t dlen = (size_t)(slash - path);
        if (dlen == 0) { dir_path[0] = '\\'; dir_path[1] = '\0'; }
        else { strncpy(dir_path, path, dlen); dir_path[dlen] = '\0'; }
        strncpy(fname, slash + 1, FAT_NAME_MAX - 1);
    } else {
        fat_getcwd(dir_path, FAT_PATH_MAX);
        strncpy(fname, path, FAT_NAME_MAX - 1);
    }
    fname[FAT_NAME_MAX - 1] = '\0';

    uint16_t dir_cl = resolve_path(dir_path);
    if (dir_cl == 0xFFFF) return -1;

    fat16_rawent_t e;
    int r = dir_find(dir_cl, fname, &e, 0, 0);
    if (r <= 0) return -1;
    if (e.attr & FAT_ATTR_DIR) return -1;

    uint32_t remaining = e.file_size;
    if (remaining >= bufsz) remaining = (uint32_t)(bufsz - 1);
    uint32_t read_so_far = 0;
    uint16_t cl = e.cluster_lo;

    while (cl < 0xFFF8 && remaining > 0) {
        uint32_t lba = cluster_to_lba(cl);
        for (uint8_t s = 0; s < spc && remaining > 0; s++) {
            uint8_t sector_buf[512];
            if (read_sector(lba + s, sector_buf) < 0) {
                buf[read_so_far] = '\0';
                return (int)read_so_far;
            }
            uint32_t to_copy = remaining < 512 ? remaining : 512;
            memcpy(buf + read_so_far, sector_buf, to_copy);
            read_so_far += to_copy;
            remaining   -= to_copy;
        }
        cl = fat_entry(cl);
    }
    buf[read_so_far] = '\0';
    return (int)read_so_far;
}

/* ---- File write (truncate then write) -------------------------------- */
static int fat_write_data(uint16_t start_cluster, const char *data, size_t len) {
    uint16_t cl = start_cluster;
    size_t   written = 0;
    while (written < len) {
        uint32_t lba = cluster_to_lba(cl);
        for (uint8_t s = 0; s < spc && written < len; s++) {
            uint8_t sector_buf[512];
            memset(sector_buf, 0, 512);
            size_t chunk = len - written;
            if (chunk > 512) chunk = 512;
            memcpy(sector_buf, data + written, chunk);
            if (write_sector(lba + s, sector_buf) < 0) return -1;
            written += chunk;
        }
        if (written < len) {
            uint16_t next = fat_entry(cl);
            if (next >= 0xFFF8) {
                uint16_t nc = alloc_cluster();
                if (!nc) return -1;
                fat_set_entry(cl, nc);
                cl = nc;
            } else {
                cl = next;
            }
        }
    }
    return 0;
}

int fat_create(const char *path) {
    if (!fs_ok) return -1;
    char dir_path[FAT_PATH_MAX], fname[FAT_NAME_MAX];
    const char *slash = 0;
    const char *tmp = path;
    while (*tmp) { if (*tmp == '\\' || *tmp == '/') slash = tmp; tmp++; }
    if (slash) {
        size_t dlen = (size_t)(slash - path);
        if (dlen == 0) { dir_path[0] = '\\'; dir_path[1] = '\0'; }
        else { strncpy(dir_path, path, dlen); dir_path[dlen] = '\0'; }
        strncpy(fname, slash + 1, FAT_NAME_MAX - 1);
    } else {
        fat_getcwd(dir_path, FAT_PATH_MAX);
        strncpy(fname, path, FAT_NAME_MAX - 1);
    }
    fname[FAT_NAME_MAX - 1] = '\0';

    uint16_t dir_cl = resolve_path(dir_path);
    if (dir_cl == 0xFFFF) return -1;

    /* Check doesn't already exist */
    fat16_rawent_t existing;
    if (dir_find(dir_cl, fname, &existing, 0, 0) > 0) return 0; /* already exists */

    char raw83[11];
    make_83(fname, raw83);

    fat16_rawent_t e;
    memset(&e, 0, sizeof(e));
    memcpy(e.name, raw83,     8);
    memcpy(e.ext,  raw83 + 8, 3);
    e.attr      = FAT_ATTR_ARCHIVE;
    e.file_size = 0;
    e.cluster_lo = 0;
    return dir_add_entry(dir_cl, &e);
}

int fat_write(const char *path, const char *data, size_t len) {
    if (!fs_ok) return -1;
    char dir_path[FAT_PATH_MAX], fname[FAT_NAME_MAX];
    const char *slash = 0;
    const char *tmp = path;
    while (*tmp) { if (*tmp == '\\' || *tmp == '/') slash = tmp; tmp++; }
    if (slash) {
        size_t dlen = (size_t)(slash - path);
        if (dlen == 0) { dir_path[0] = '\\'; dir_path[1] = '\0'; }
        else { strncpy(dir_path, path, dlen); dir_path[dlen] = '\0'; }
        strncpy(fname, slash + 1, FAT_NAME_MAX - 1);
    } else {
        fat_getcwd(dir_path, FAT_PATH_MAX);
        strncpy(fname, path, FAT_NAME_MAX - 1);
    }
    fname[FAT_NAME_MAX - 1] = '\0';

    uint16_t dir_cl = resolve_path(dir_path);
    if (dir_cl == 0xFFFF) return -1;

    fat16_rawent_t e;
    uint32_t ent_lba; int ent_idx;
    int r = dir_find(dir_cl, fname, &e, &ent_lba, &ent_idx);
    if (r <= 0) {
        /* Create */
        if (fat_create(path) < 0) return -1;
        r = dir_find(dir_cl, fname, &e, &ent_lba, &ent_idx);
        if (r <= 0) return -1;
    }

    /* Allocate cluster if needed */
    uint16_t cl = e.cluster_lo;
    if (cl == 0 && len > 0) {
        cl = alloc_cluster();
        if (!cl) return -1;
    }

    if (len > 0 && fat_write_data(cl, data, len) < 0) return -1;

    /* Update directory entry */
    uint8_t buf[512];
    if (read_sector(ent_lba, buf) < 0) return -1;
    fat16_rawent_t *ep = (fat16_rawent_t *)buf + ent_idx;
    ep->cluster_lo = cl;
    ep->file_size  = (uint32_t)len;
    return write_sector(ent_lba, buf);
}

int fat_append(const char *path, const char *data, size_t len) {
    if (!fs_ok) return -1;
    /* Read existing content, append, re-write */
    char tmp_buf[4096];
    int existing = fat_read(path, tmp_buf, sizeof(tmp_buf) - len - 1);
    if (existing < 0) existing = 0;
    memcpy(tmp_buf + existing, data, len);
    return fat_write(path, tmp_buf, (size_t)existing + len);
}

int fat_mkdir(const char *path) {
    if (!fs_ok) return -1;
    char dir_path[FAT_PATH_MAX], dname[FAT_NAME_MAX];
    const char *slash = 0;
    const char *tmp = path;
    while (*tmp) { if (*tmp == '\\' || *tmp == '/') slash = tmp; tmp++; }
    if (slash) {
        size_t dlen = (size_t)(slash - path);
        if (dlen == 0) { dir_path[0] = '\\'; dir_path[1] = '\0'; }
        else { strncpy(dir_path, path, dlen); dir_path[dlen] = '\0'; }
        strncpy(dname, slash + 1, FAT_NAME_MAX - 1);
    } else {
        fat_getcwd(dir_path, FAT_PATH_MAX);
        strncpy(dname, path, FAT_NAME_MAX - 1);
    }
    dname[FAT_NAME_MAX - 1] = '\0';

    uint16_t dir_cl = resolve_path(dir_path);
    if (dir_cl == 0xFFFF) return -1;

    uint16_t nc = alloc_cluster();
    if (!nc) return -1;

    char raw83[11];
    make_83(dname, raw83);

    fat16_rawent_t e;
    memset(&e, 0, sizeof(e));
    memcpy(e.name, raw83,     8);
    memcpy(e.ext,  raw83 + 8, 3);
    e.attr       = FAT_ATTR_DIR;
    e.cluster_lo = nc;
    e.file_size  = 0;
    return dir_add_entry(dir_cl, &e);
}

int fat_unlink(const char *path) {
    if (!fs_ok) return -1;
    char dir_path[FAT_PATH_MAX], fname[FAT_NAME_MAX];
    const char *slash = 0;
    const char *tmp = path;
    while (*tmp) { if (*tmp == '\\' || *tmp == '/') slash = tmp; tmp++; }
    if (slash) {
        size_t dlen = (size_t)(slash - path);
        if (dlen == 0) { dir_path[0] = '\\'; dir_path[1] = '\0'; }
        else { strncpy(dir_path, path, dlen); dir_path[dlen] = '\0'; }
        strncpy(fname, slash + 1, FAT_NAME_MAX - 1);
    } else {
        fat_getcwd(dir_path, FAT_PATH_MAX);
        strncpy(fname, path, FAT_NAME_MAX - 1);
    }
    fname[FAT_NAME_MAX - 1] = '\0';

    uint16_t dir_cl = resolve_path(dir_path);
    if (dir_cl == 0xFFFF) return -1;

    uint32_t ent_lba; int ent_idx;
    fat16_rawent_t e;
    int r = dir_find(dir_cl, fname, &e, &ent_lba, &ent_idx);
    if (r <= 0) return -1;

    /* Free cluster chain */
    uint16_t cl = e.cluster_lo;
    while (cl >= 2 && cl < 0xFFF8) {
        uint16_t next = fat_entry(cl);
        fat_set_entry(cl, 0x0000);
        cl = next;
    }

    /* Mark entry as deleted */
    uint8_t buf[512];
    if (read_sector(ent_lba, buf) < 0) return -1;
    buf[ent_idx * 32] = 0xE5;
    return write_sector(ent_lba, buf);
}

int fat_rmdir(const char *path) {
    return fat_unlink(path);
}
