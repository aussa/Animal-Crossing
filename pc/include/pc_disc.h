/* pc_disc.h - GameCube disc image reader (CISO/ISO/GCM) */
#ifndef PC_DISC_H
#define PC_DISC_H

#include "types.h"

/* Initialize disc image reader. Searches for .ciso/.iso/.gcm in
 * current dir, orig/, rom/. Parses GCM filesystem into lookup table.
 * Returns 1 on success, 0 if no disc image found. */
int pc_disc_init(void);

/* Check if a disc image is open. */
int pc_disc_is_open(void);

/* Find a file in the disc FST by path (e.g., "COPYDATE", "audiores/banks/bank0.aw").
 * Leading '/' is stripped automatically. Returns 1 if found. */
int pc_disc_find_file(const char* path, u32* disc_offset, u32* file_size);

/* Read bytes from a logical disc offset. Returns 1 on success. */
int pc_disc_read(u32 offset, void* dest, u32 size);

/* Extract DOL and REL as malloc'd buffers (for pc_assets.c). */
u8* pc_disc_extract_dol(void);
u8* pc_disc_extract_rel(void); /* handles Yaz0 decompression */
/* Sized variants — also write the buffer's byte count to *out_size. */
u8* pc_disc_extract_dol_sized(unsigned int* out_size);
u8* pc_disc_extract_rel_sized(unsigned int* out_size);

/* Returns the null-terminated 6-byte game ID from the disc header (e.g. "GAFE01"),
 * or an empty string if no disc is open. */
const char* pc_disc_get_game_id(void);

/* Scan standard disc dirs for an ISO/GCM whose first 4 bytes match gameid4.
 * Writes path to out_path (max out_sz chars). Returns 1 on success. */
int pc_disc_find_by_gameid(const char* gameid4, char* out_path, int out_sz);

/* Open iso_path, find filename (basename match) in its GCM FST, and read
 * it into a malloc'd buffer. Caller must free(*out_buf). Returns 1 on success. */
int pc_disc_load_from_iso(const char* iso_path, const char* filename, u8** out_buf, u32* out_size);

/* Close disc image and free resources. */
void pc_disc_shutdown(void);

#endif /* PC_DISC_H */
