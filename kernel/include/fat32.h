#ifndef CARSONOS_FAT32_H
#define CARSONOS_FAT32_H
#include <stdint.h>
int fs_init(void);
int fs_is_mounted(void);
int fs_read_file(const char *path,uint8_t *buffer,uint32_t capacity,uint32_t *size_out);
uint32_t fs_volume_sectors(void);
const char *fs_status(void);
#endif
