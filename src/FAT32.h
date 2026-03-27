#ifndef KERNEL_FAT32_H
#define KERNEL_FAT32_H

#include "lib.h"
#include "io.h"
#include "ata.h"

typedef struct {
    uint32_t fat_start;       // LBA of FAT
    uint32_t data_start;      // LBA of data region
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
    uint32_t root_cluster;    // usually 2
    uint32_t total_clusters;
    uint8_t *fat_table;       // FAT loaded into memory
} FAT32_FS;

typedef struct {
    uint32_t first_cluster;
    uint32_t size;
} FAT32_File;

typedef struct {
    char name[11];       // 8+3
    uint8_t attr;
    uint32_t first_cluster_hi;
    uint32_t first_cluster_lo;
    uint32_t size;
} __attribute__((packed)) FAT32_DirEntry;

// FAT attributes
#define FAT_ATTR_DIR  0x10
#define FAT_ATTR_FILE 0x20

static inline int disk_read(uint32_t lba, uint32_t count, void *buffer) {
    for (uint32_t i = 0; i < count; i++) {
        // Wait until device is ready
        while (inb(ATA_REG_STATUS) & ATA_SR_BSY);

        // Select drive (master) and LBA bits 24-27
        outb(ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));

        outb(ATA_REG_SECCOUNT0, 1);           // sectors to read
        outb(ATA_REG_LBA0, lba & 0xFF);
        outb(ATA_REG_LBA1, (lba >> 8) & 0xFF);
        outb(ATA_REG_LBA2, (lba >> 16) & 0xFF);
        outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        // Wait for DRQ
        while (!(inb(ATA_REG_STATUS) & ATA_SR_DRQ));

        // Read 512 bytes (256 words) from data port
        insw(ATA_REG_DATA, buffer + i * 512, 256);

        lba++;
    }
    return 0;
}

static inline int disk_write(uint32_t lba, uint32_t count, const void *buffer) {
    for (uint32_t i = 0; i < count; i++) {
        while (inb(ATA_REG_STATUS) & ATA_SR_BSY);

        outb(ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_REG_SECCOUNT0, 1);
        outb(ATA_REG_LBA0, lba & 0xFF);
        outb(ATA_REG_LBA1, (lba >> 8) & 0xFF);
        outb(ATA_REG_LBA2, (lba >> 16) & 0xFF);
        outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        while (!(inb(ATA_REG_STATUS) & ATA_SR_DRQ));

        insw(ATA_REG_DATA, (void*)(buffer + i*512), 256); // some implementations use outsw
        lba++;
    }
    return 0;
}


static inline uint32_t next_cluster(FAT32_FS *fs, uint32_t cluster) {
    uint32_t entry = ((uint32_t*)fs->fat_table)[cluster];
    if (entry >= 0x0FFFFFF8) return 0;
    return entry;
}

static inline uint32_t cluster_to_lba(FAT32_FS *fs, uint32_t cluster) {
    return fs->data_start + (cluster - 2) * fs->sectors_per_cluster;
}

static inline int read_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer) {
    return disk_read(cluster_to_lba(fs, cluster), fs->sectors_per_cluster, buffer);
}

static inline uint32_t find_free_cluster(FAT32_FS *fs) {
    for (uint32_t i = 2; i < fs->total_clusters; i++) {
        if (((uint32_t*)fs->fat_table)[i] == 0)
            return i;
    }
    return 0;
}

// Allocate N clusters and return first cluster
static inline uint32_t allocate_clusters(FAT32_FS *fs, uint32_t n) {
    uint32_t first = 0, prev = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = find_free_cluster(fs);
        if (!c) return 0; // no space
        ((uint32_t*)fs->fat_table)[c] = 0x0FFFFFFF;
        if (prev) ((uint32_t*)fs->fat_table)[prev] = c;
        else first = c;
        prev = c;
    }
    return first;
}

// Find directory entry in a directory cluster chain
static FAT32_DirEntry* find_dir_entry_in_cluster(FAT32_FS *fs, uint32_t dir_cluster, const char *name, uint8_t *sector_buffer) {
    uint32_t cluster = dir_cluster;
    do {
        read_cluster(fs, cluster, sector_buffer);
        for (uint32_t i = 0; i < fs->bytes_per_sector * fs->sectors_per_cluster; i += sizeof(FAT32_DirEntry)) {
            FAT32_DirEntry *entry = (FAT32_DirEntry*)(sector_buffer + i);
            if (entry->name[0] == 0 || entry->name[0] == 0xE5) continue;
            if (memcmp_custom(entry->name, name, 11) == 0) return entry;
        }
        cluster = next_cluster(fs, cluster);
    } while(cluster);
    return 0;
}

// Split path into components, simple '/' separated, no LFN
static int split_path(char *path, char components[][12], int max_components) {
    int count = 0;
    char *p = path;
    while (*p && count < max_components) {
        while (*p == '/') p++; // skip slashes
        if (!*p) break;
        int i = 0;
        while (*p && *p != '/' && i < 11) {
            components[count][i++] = *p++;
        }
        while (i < 11) components[count][i++] = ' ';
        count++;
    }
    return count;
}

// Create a folder in a parent directory
static int fat32_create_folder(FAT32_FS *fs, uint32_t parent_cluster, const char *name) {
    uint32_t cluster = allocate_clusters(fs, 1);
    if (!cluster) return 0;

    uint8_t sector[fs->bytes_per_sector * fs->sectors_per_cluster];
    memset_custom(sector, 0, sizeof(sector));

    FAT32_DirEntry entry;
    memset_custom(&entry, ' ', sizeof(entry));
    memcpy_custom(entry.name, name, strlen_custom(name) > 11 ? 11 : strlen_custom(name));
    entry.attr = FAT_ATTR_DIR;
    entry.first_cluster_hi = (cluster >> 16) & 0xFFFF;
    entry.first_cluster_lo = cluster & 0xFFFF;
    entry.size = 0;

    uint32_t lba = cluster_to_lba(fs, parent_cluster);
    read_cluster(fs, parent_cluster, sector); // read parent cluster
    memcpy_custom(sector, &entry, sizeof(entry)); // write entry in first slot
    disk_write(lba, fs->sectors_per_cluster, sector);

    disk_write(fs->fat_start, fs->total_clusters * 4 / fs->bytes_per_sector, fs->fat_table);
    return 1;
}

// Resolve a path to a cluster
static uint32_t resolve_path(FAT32_FS *fs, char *path, uint8_t *sector_buffer, FAT32_DirEntry *out_entry) {
    char components[8][12];
    int count = split_path(path, components, 8);
    uint32_t cluster = fs->root_cluster;
    FAT32_DirEntry *entry = 0;

    for (int i = 0; i < count; i++) {
        entry = find_dir_entry_in_cluster(fs, cluster, components[i], sector_buffer);
        if (!entry) return 0;
        cluster = (entry->first_cluster_hi << 16) | entry->first_cluster_lo;
    }

    if (out_entry) *out_entry = *entry;
    return cluster;
}

// Read a file from a path
static int fat32_read_file_path(FAT32_FS *fs, const char *path, uint8_t *buffer, uint32_t max_size) {
    uint8_t sector[fs->bytes_per_sector * fs->sectors_per_cluster];
    FAT32_DirEntry entry;
    uint32_t cluster = resolve_path(fs, (char*)path, sector, &entry);
    if (!cluster) return 0;

    uint32_t size = entry.size;
    if (size > max_size) size = max_size;

    uint32_t bytes_read = 0;
    while (cluster && bytes_read < size) {
        read_cluster(fs, cluster, sector);
        uint32_t to_copy = fs->sectors_per_cluster * fs->bytes_per_sector;
        if (bytes_read + to_copy > size) to_copy = size - bytes_read;
        memcpy_custom(buffer + bytes_read, sector, to_copy);
        bytes_read += to_copy;
        cluster = next_cluster(fs, cluster);
    }
    return bytes_read;
}

// Write a file to a path, create if missing
static int fat32_write_file_path(FAT32_FS *fs, const char *path, const uint8_t *data, uint32_t size) {
    char components[8][12];
    int count = split_path((char*)path, components, 8);
    uint8_t sector[fs->bytes_per_sector * fs->sectors_per_cluster];
    uint32_t parent_cluster = fs->root_cluster;
    FAT32_DirEntry entry;

    // Traverse/create directories
    for (int i = 0; i < count - 1; i++) {
        if (!find_dir_entry_in_cluster(fs, parent_cluster, components[i], sector)) {
            fat32_create_folder(fs, parent_cluster, components[i]);
        }
        entry = *find_dir_entry_in_cluster(fs, parent_cluster, components[i], sector);
        parent_cluster = (entry.first_cluster_hi << 16) | entry.first_cluster_lo;
    }

    // Allocate clusters for file
    uint32_t clusters_needed = (size + fs->sectors_per_cluster * fs->bytes_per_sector - 1)
                              / (fs->sectors_per_cluster * fs->bytes_per_sector);
    uint32_t first_cluster = allocate_clusters(fs, clusters_needed);
    if (!first_cluster) return 0;

    // Write file data
    uint32_t bytes_written = 0;
    uint32_t cluster = first_cluster;
    while (cluster && bytes_written < size) {
        uint32_t to_write = fs->sectors_per_cluster * fs->bytes_per_sector;
        if (bytes_written + to_write > size) to_write = size - bytes_written;
        memset_custom(sector, 0, sizeof(sector));
        memcpy_custom(sector, data + bytes_written, to_write);
        disk_write(cluster_to_lba(fs, cluster), fs->sectors_per_cluster, sector);
        bytes_written += to_write;
        cluster = next_cluster(fs, cluster);
    }

    // Write directory entry in parent
    FAT32_DirEntry file_entry;
    memset_custom(&file_entry, ' ', sizeof(file_entry));
    memcpy_custom(file_entry.name, components[count - 1], strlen_custom(components[count - 1]));
    file_entry.first_cluster_hi = (first_cluster >> 16) & 0xFFFF;
    file_entry.first_cluster_lo = first_cluster & 0xFFFF;
    file_entry.size = size;
    file_entry.attr = FAT_ATTR_FILE;
    // write into first free slot
    read_cluster(fs, parent_cluster, sector);
    memcpy_custom(sector, &file_entry, sizeof(file_entry));
    disk_write(cluster_to_lba(fs, parent_cluster), fs->sectors_per_cluster, sector);

    // Update FAT
    disk_write(fs->fat_start, fs->total_clusters * 4 / fs->bytes_per_sector, fs->fat_table);
    return 1;
}

#endif
