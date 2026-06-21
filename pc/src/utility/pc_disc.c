/* Read files from GC disc images (CISO/ISO/GCM)
 * Used by pc_assets.c for DOL+REL extraction and pc_dvd.c for runtime file reads. */
#ifdef TARGET_PC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "types.h"
#include "pc_disc.h"

extern int g_pc_verbose;

/* ---- endian helpers ---- */
static u32 be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
static u32 le32(const u8* p) {
    return p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* ---- CISO format ---- */
#define CISO_HDR_SIZE 0x8000
#define CISO_MAGIC    0x4F534943 /* "CISO" as LE u32 */
#define CISO_MAP_OFF  8

typedef struct {
    FILE* fp;
    int is_ciso;
    u32 block_size;
    int num_blocks;
    int* block_phys; /* logical block -> physical block, -1 = absent */
} DiscFile;

/* ---- global state ---- */
static DiscFile g_disc;
static int g_disc_open = 0;
static char g_disc_game_id[7] = {0};

/* DOL info */
static u32 g_dol_offset = 0;
static u32 g_dol_size = 0;

/* FST file table */
#define MAX_FST_FILES 1024
typedef struct {
    char path[256];
    u32 disc_offset;
    u32 file_size;
} FSTFile;
static FSTFile g_fst_files[MAX_FST_FILES];
static int g_fst_file_count = 0;

/* ---- disc I/O ---- */
static int disc_open(DiscFile* df, const char* path) {
    u8 hdr[CISO_HDR_SIZE];

    memset(df, 0, sizeof(*df));
    df->fp = fopen(path, "rb");
    if (!df->fp) return 0;

    /* try CISO */
    if (fread(hdr, 1, CISO_HDR_SIZE, df->fp) == CISO_HDR_SIZE &&
        le32(hdr) == CISO_MAGIC) {
        df->block_size = le32(hdr + 4);
        if (df->block_size > 0) {
            int i, phys = 0;
            df->num_blocks = CISO_HDR_SIZE - CISO_MAP_OFF;
            df->block_phys = (int*)malloc(df->num_blocks * sizeof(int));
            for (i = 0; i < df->num_blocks; i++)
                df->block_phys[i] = hdr[CISO_MAP_OFF + i] ? phys++ : -1;
            df->is_ciso = 1;
            return 1;
        }
    }

    /* plain ISO/GCM */
    df->is_ciso = 0;
    return 1;
}

static void disc_close(DiscFile* df) {
    if (df->fp) fclose(df->fp);
    if (df->block_phys) free(df->block_phys);
    memset(df, 0, sizeof(*df));
}

static int disc_read(DiscFile* df, u32 offset, void* dest, u32 size) {
    if (!df->is_ciso) {
        fseek(df->fp, (long)offset, SEEK_SET);
        return (u32)fread(dest, 1, size, df->fp) == size;
    }

    {
        u8* out = (u8*)dest;
        while (size > 0) {
            u32 bi = offset / df->block_size;
            u32 bo = offset % df->block_size;
            u32 chunk = df->block_size - bo;
            if (chunk > size) chunk = size;

            if ((int)bi >= df->num_blocks || df->block_phys[bi] < 0) {
                memset(out, 0, chunk);
            } else {
                u32 phys = CISO_HDR_SIZE +
                    (u32)df->block_phys[bi] * df->block_size + bo;
                fseek(df->fp, (long)phys, SEEK_SET);
                if ((u32)fread(out, 1, chunk, df->fp) != chunk) return 0;
            }

            out += chunk;
            offset += chunk;
            size -= chunk;
        }
    }
    return 1;
}

/* ---- Yaz0 (SZS) decompression ---- */
static u8* yaz0_decode(const u8* src, u32 src_size, u32* out_size) {
    u32 dec_size, sp, dp;
    u8* dst;
    int bit;

    if (src_size < 16 || memcmp(src, "Yaz0", 4) != 0) return NULL;

    dec_size = be32(src + 4);
    dst = (u8*)malloc(dec_size);
    if (!dst) return NULL;

    sp = 16;
    dp = 0;
    while (dp < dec_size && sp < src_size) {
        u8 flags = src[sp++];
        for (bit = 7; bit >= 0 && dp < dec_size; bit--) {
            if (sp >= src_size) break;
            if (flags & (1 << bit)) {
                dst[dp++] = src[sp++];
            } else {
                u8 b1, b2;
                u32 dist, len, ref;
                if (sp + 1 >= src_size) break;
                b1 = src[sp++];
                b2 = src[sp++];
                dist = ((u32)(b1 & 0x0F) << 8) | b2;
                if ((b1 >> 4) == 0) {
                    if (sp >= src_size) break;
                    len = (u32)src[sp++] + 0x12;
                } else {
                    len = (u32)(b1 >> 4) + 2;
                }
                ref = dp - dist - 1;
                while (len-- > 0 && dp < dec_size)
                    dst[dp++] = dst[ref++];
            }
        }
    }

    *out_size = dec_size;
    return dst;
}

/* ---- GCM header parsing ---- */
#define GC_MAGIC 0xC2339F3D

static int gcm_verify(DiscFile* df) {
    u8 buf[4];
    disc_read(df, 0x1C, buf, 4);
    return be32(buf) == GC_MAGIC;
}

static u32 gcm_dol_offset_read(DiscFile* df) {
    u8 buf[4];
    disc_read(df, 0x420, buf, 4);
    return be32(buf);
}

static u32 gcm_dol_size_calc(DiscFile* df, u32 dol_off) {
    u8 hdr[0xE4];
    u32 max_end = 0;
    int i;

    disc_read(df, dol_off, hdr, 0xE4);

    for (i = 0; i < 7; i++) {
        u32 off = be32(hdr + i * 4);
        u32 sz  = be32(hdr + 0x90 + i * 4);
        if (off + sz > max_end) max_end = off + sz;
    }
    for (i = 0; i < 11; i++) {
        u32 off = be32(hdr + 0x1C + i * 4);
        u32 sz  = be32(hdr + 0xAC + i * 4);
        if (off + sz > max_end) max_end = off + sz;
    }
    return max_end;
}

/* ---- FST path table builder ---- */
static void build_fst_table(DiscFile* df) {
    u8 buf[12];
    u32 fst_off, num_ent, str_tbl;
    u32 i;

    /* directory stack for building full paths */
    struct { u32 next_entry; char name[128]; } dir_stack[32];
    int stack_depth = 0;

    g_fst_file_count = 0;

    disc_read(df, 0x424, buf, 4);
    fst_off = be32(buf);

    disc_read(df, fst_off + 8, buf, 4);
    num_ent = be32(buf);
    str_tbl = fst_off + num_ent * 12;

    /* push root */
    dir_stack[0].next_entry = num_ent;
    dir_stack[0].name[0] = '\0';
    stack_depth = 1;

    for (i = 1; i < num_ent; i++) {
        u32 noff;
        char name[128];

        /* pop directories we've passed */
        while (stack_depth > 0 && i >= dir_stack[stack_depth - 1].next_entry)
            stack_depth--;

        disc_read(df, fst_off + i * 12, buf, 12);
        noff = ((u32)buf[1] << 16) | ((u32)buf[2] << 8) | buf[3];
        disc_read(df, str_tbl + noff, name, 127);
        name[127] = '\0';

        if (buf[0] == 1) {
            /* directory: push onto stack */
            if (stack_depth < 32) {
                dir_stack[stack_depth].next_entry = be32(buf + 8);
                strncpy(dir_stack[stack_depth].name, name, 127);
                dir_stack[stack_depth].name[127] = '\0';
                stack_depth++;
            }
        } else {
            /* file: build full path from directory stack */
            if (g_fst_file_count < MAX_FST_FILES) {
                char path[256];
                int d;
                path[0] = '\0';
                for (d = 1; d < stack_depth; d++) {
                    strncat(path, dir_stack[d].name,
                            sizeof(path) - strlen(path) - 2);
                    strcat(path, "/");
                }
                strncat(path, name, sizeof(path) - strlen(path) - 1);

                strncpy(g_fst_files[g_fst_file_count].path, path, 255);
                g_fst_files[g_fst_file_count].path[255] = '\0';
                g_fst_files[g_fst_file_count].disc_offset = be32(buf + 4);
                g_fst_files[g_fst_file_count].file_size = be32(buf + 8);
                g_fst_file_count++;
            }
        }
    }

    if (g_pc_verbose) {
        printf("[PC] FST: %d files indexed\n", g_fst_file_count);
        for (int fi = 0; fi < g_fst_file_count; fi++)
            printf("[PC] FST[%d]: %s (%u bytes @ 0x%X)\n", fi,
                   g_fst_files[fi].path, g_fst_files[fi].file_size,
                   g_fst_files[fi].disc_offset);
    }
}

/* ---- disc image search ---- */
static int str_ends_ci(const char* s, const char* suffix) {
    size_t sl = strlen(s), el = strlen(suffix);
    const char* p;
    if (sl < el) return 0;
    p = s + sl - el;
    while (*suffix) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*suffix))
            return 0;
        p++;
        suffix++;
    }
    return 1;
}

/* Preferred 4-byte game ID prefix for the compiled version */
#if VERSION == VER_GAFJ01_00
#define PREFERRED_GAME_ID "GAEJ"  /* Japan disc header: GAEJ01 (Doubutsu no Mori e+) */
#elif VERSION == VER_GAFU01_00
#define PREFERRED_GAME_ID "GAFU"
#else
#define PREFERRED_GAME_ID "GAFE"
#endif

static int find_disc_image(char* out_path, int out_sz) {
    static const char* dirs[] = { ".", "orig", "rom", NULL };
    char fallback[512];
    int fallback_found = 0;
    int d;

    fallback[0] = '\0';

    for (d = 0; dirs[d]; d++) {
        DIR* dp = opendir(dirs[d]);
        struct dirent* ent;
        if (!dp) continue;

        while ((ent = readdir(dp)) != NULL) {
            char cand[512];
            DiscFile df;
            u8 buf[4];
            char gameid[5];
            int ok;

            if (!str_ends_ci(ent->d_name, ".ciso") &&
                !str_ends_ci(ent->d_name, ".iso")  &&
                !str_ends_ci(ent->d_name, ".gcm")) continue;

            if (strcmp(dirs[d], ".") == 0)
                snprintf(cand, sizeof(cand), "%s", ent->d_name);
            else
                snprintf(cand, sizeof(cand), "%s/%s", dirs[d], ent->d_name);

            /* Peek at game ID to find the preferred disc */
            ok = 0;
            if (disc_open(&df, cand)) {
                if (disc_read(&df, 0, buf, 4)) {
                    memcpy(gameid, buf, 4);
                    gameid[4] = '\0';
                    ok = 1;
                }
                disc_close(&df);
            }
            if (!ok) continue;

            if (strncmp(gameid, PREFERRED_GAME_ID, 4) == 0) {
                snprintf(out_path, out_sz, "%s", cand);
                closedir(dp);
                return 1;
            }

            if (!fallback_found) {
                snprintf(fallback, sizeof(fallback), "%s", cand);
                fallback_found = 1;
            }
        }
        closedir(dp);
    }

    if (fallback_found) {
        snprintf(out_path, out_sz, "%s", fallback);
        return 1;
    }
    return 0;
}

/* ---- public API ---- */

int pc_disc_init(void) {
    char path[512];

    if (g_disc_open) return 1;
    if (!find_disc_image(path, sizeof(path))) {
        fprintf(stderr, "[PC] no disc image found (searched ., orig/, rom/)\n");
        return 0;
    }
    if (!disc_open(&g_disc, path)) {
        fprintf(stderr, "[PC] failed to open disc image: %s\n", path);
        return 0;
    }

    if (!gcm_verify(&g_disc)) {
        fprintf(stderr, "[PC] %s: not a valid GC disc image\n", path);
        disc_close(&g_disc);
        return 0;
    }

    {
        disc_read(&g_disc, 0, g_disc_game_id, 6);
        g_disc_game_id[6] = '\0';
        fprintf(stderr, "[PC] disc image: %s (%s, game ID: %s)\n",
            path, g_disc.is_ciso ? "CISO" : "ISO/GCM", g_disc_game_id);
    }

    /* cache DOL info */
    g_dol_offset = gcm_dol_offset_read(&g_disc);
    g_dol_size = gcm_dol_size_calc(&g_disc, g_dol_offset);

    /* build FST lookup table */
    build_fst_table(&g_disc);

    g_disc_open = 1;
    return 1;
}

int pc_disc_is_open(void) {
    return g_disc_open;
}

int pc_disc_find_file(const char* path, u32* disc_offset, u32* file_size) {
    int i;
    if (!g_disc_open) return 0;

    /* strip leading slash */
    if (path[0] == '/') path++;

    for (i = 0; i < g_fst_file_count; i++) {
        if (strcmp(g_fst_files[i].path, path) == 0) {
            *disc_offset = g_fst_files[i].disc_offset;
            *file_size = g_fst_files[i].file_size;
            return 1;
        }
    }
    return 0;
}

int pc_disc_read(u32 offset, void* dest, u32 size) {
    if (!g_disc_open) return 0;
    return disc_read(&g_disc, offset, dest, size);
}

u8* pc_disc_extract_dol(void) {
    u8* buf;
    if (!g_disc_open) return NULL;
    buf = (u8*)malloc(g_dol_size);
    if (!buf) return NULL;
    if (!disc_read(&g_disc, g_dol_offset, buf, g_dol_size)) {
        free(buf);
        return NULL;
    }
    fprintf(stderr, "[PC] DOL: %u bytes (offset 0x%X)\n", g_dol_size, g_dol_offset);
    return buf;
}

u8* pc_disc_extract_dol_sized(unsigned int* out_size) {
    u8* buf = pc_disc_extract_dol();
    if (out_size) *out_size = buf ? g_dol_size : 0;
    return buf;
}

u8* pc_disc_extract_rel(void) {
    u32 off, sz;
    u8* raw;

    if (!pc_disc_find_file("foresta.rel.szs", &off, &sz)) {
        fprintf(stderr, "[PC] foresta.rel.szs not found in disc FST\n");
        return NULL;
    }

    raw = (u8*)malloc(sz);
    if (!raw) return NULL;
    if (!disc_read(&g_disc, off, raw, sz)) {
        free(raw);
        return NULL;
    }

    /* Yaz0 decompression if needed */
    if (sz >= 16 && memcmp(raw, "Yaz0", 4) == 0) {
        u32 dec_sz;
        u8* dec = yaz0_decode(raw, sz, &dec_sz);
        free(raw);
        if (!dec) {
            fprintf(stderr, "[PC] Yaz0 decompression failed\n");
            return NULL;
        }
        fprintf(stderr, "[PC] REL: %u bytes (Yaz0 %u -> %u)\n", dec_sz, sz, dec_sz);
        return dec;
    }

    fprintf(stderr, "[PC] REL: %u bytes (raw)\n", sz);
    return raw;
}

u8* pc_disc_extract_rel_sized(unsigned int* out_size) {
    /* We don't have the decompressed size until after the call, so call and
     * then recover size from the Yaz0 header embedded in the decompressed data,
     * or use the raw size. Simpler: store it in a local and pass out. */
    u32 off, sz;
    if (!pc_disc_find_file("foresta.rel.szs", &off, &sz)) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    /* Peek at Yaz0 header to get decompressed size */
    if (out_size) {
        u8 peek[8];
        if (disc_read(&g_disc, off, peek, 8) && memcmp(peek, "Yaz0", 4) == 0)
            *out_size = ((u32)peek[4]<<24)|((u32)peek[5]<<16)|((u32)peek[6]<<8)|peek[7];
        else
            *out_size = sz;
    }
    return pc_disc_extract_rel();
}

const char* pc_disc_get_game_id(void) {
    return g_disc_game_id;
}

int pc_disc_find_by_gameid(const char* gameid4, char* out_path, int out_sz) {
    static const char* dirs[] = { ".", "orig", "rom", NULL };
    int d;
    for (d = 0; dirs[d]; d++) {
        DIR* dp = opendir(dirs[d]);
        struct dirent* ent;
        if (!dp) continue;
        while ((ent = readdir(dp)) != NULL) {
            char cand[512];
            DiscFile df;
            u8 buf[4];
            int ok = 0;
            if (!str_ends_ci(ent->d_name, ".ciso") &&
                !str_ends_ci(ent->d_name, ".iso")  &&
                !str_ends_ci(ent->d_name, ".gcm")) continue;
            if (strcmp(dirs[d], ".") == 0)
                snprintf(cand, sizeof(cand), "%s", ent->d_name);
            else
                snprintf(cand, sizeof(cand), "%s/%s", dirs[d], ent->d_name);
            if (disc_open(&df, cand)) {
                if (disc_read(&df, 0, buf, 4))
                    ok = (memcmp(buf, gameid4, 4) == 0);
                disc_close(&df);
            }
            if (ok) {
                snprintf(out_path, out_sz, "%s", cand);
                closedir(dp);
                return 1;
            }
        }
        closedir(dp);
    }
    return 0;
}

int pc_disc_load_from_iso(const char* iso_path, const char* filename, u8** out_buf, u32* out_size) {
    DiscFile df;
    u8 hdr[0x460];
    u32 fst_off, fst_sz, num_root, str_base, i;
    u8* fst = NULL;
    u8* fbuf = NULL;
    u32 file_off = 0, file_sz = 0;
    int found = 0;

    *out_buf = NULL;
    *out_size = 0;

    if (!disc_open(&df, iso_path)) return 0;
    if (!disc_read(&df, 0, hdr, sizeof(hdr))) goto fail;

    fst_off  = be32(hdr + 0x424);
    fst_sz   = be32(hdr + 0x428);
    fst = (u8*)malloc(fst_sz);
    if (!fst || !disc_read(&df, fst_off, fst, fst_sz)) goto fail;

    num_root = be32(fst + 8);
    str_base = num_root * 12;

    for (i = 1; i < num_root; i++) {
        u8* e = fst + i * 12;
        if (e[0] != 0) continue; /* skip directories */
        {
            u32 name_off = be32(e) & 0xFFFFFF;
            const char* name = (const char*)(fst + str_base + name_off);
            if (strcmp(name, filename) == 0) {
                file_off = be32(e + 4);
                file_sz  = be32(e + 8);
                found = 1;
                break;
            }
        }
    }
    if (!found) goto fail;

    fbuf = (u8*)malloc(file_sz);
    if (!fbuf || !disc_read(&df, file_off, fbuf, file_sz)) {
        free(fbuf); fbuf = NULL;
        goto fail;
    }

    free(fst);
    disc_close(&df);
    *out_buf = fbuf;
    *out_size = file_sz;
    return 1;

fail:
    free(fst);
    disc_close(&df);
    return 0;
}

void pc_disc_shutdown(void) {
    if (g_disc_open) {
        disc_close(&g_disc);
        g_disc_open = 0;
        g_fst_file_count = 0;
        g_disc_game_id[0] = '\0';
    }
}

#endif /* TARGET_PC */
