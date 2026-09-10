#ifndef LF2_ASSET_PACK_H
#define LF2_ASSET_PACK_H
#include <stddef.h>
#include <stdint.h>
int lf2_pak_init(const char *path);
void lf2_pak_shutdown(void);
int lf2_pak_count(void);
const char *lf2_pak_entry_name(int index);
size_t lf2_pak_entry_size(int index);
uint32_t lf2_pak_entry_crc(int index);
int lf2_pak_read(const char *relpath, void **data_out, size_t *size_out);
int lf2_pak_read_index(int index, void **data_out, size_t *size_out);
int lf2_pak_verify_index(int index, char *error, int error_cap);
#endif
