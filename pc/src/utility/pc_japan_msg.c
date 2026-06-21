/* pc_japan_msg.c - Overlay Japanese text from Doubutsu no Mori e+ (Japan ISO).
 *
 * The game boots with the USA ISO normally. At startup this module scans for a
 * second ISO whose game ID starts with "GAEJ", loads msg.bin from its
 * forest_msg.arc, parses the BMG (MESGbmg1) format, and stores the INF1 entry
 * table and DAT1 string data in RAM.
 *
 * pc_japan_msg_copy(idx, dst, max, out_size) is then called from mMsg_LoadMsgData
 * (m_msg_main.c_inc) before the normal ARAM path, short-circuiting it with a
 * transcoded internal representation when available.
 *
 * Japan BMG encoding (Doubutsu no Mori e+):
 *   DAT1 uses a custom single-byte character map (NOT Shift-JIS).
 *   Each byte 0x00-0xFF maps to a Unicode character per the DnMe+ char table.
 *   Special bytes:
 *     0x7F ID [params]  - control code; ID in 0x00-0x7C, size from japan_cont_size_tbl
 *     0x80 SIZE [data]  - MessageTag (player/town name substitution etc.);
 *                         SIZE = total tag bytes including the 0x80 byte itself
 *     0xCD              - newline (same value passes through to renderer)
 *     0x20              - space (passes through to renderer)
 *
 * Internal encoding emitted by pc_japan_msg_copy():
 *   0xFE 0x00 C  - 3-byte glyph token; C = Japan char map byte (= atlas slot index)
 *   0xCD         - CHAR_NEW_LINE (same value in USA system)
 *   0x20         - space
 *   0x7F 0x00    - end-of-message (CHAR_CONTROL_CODE + mFont_CONT_CODE_LAST)
 * Japan page breaks (MessageTag grp=01 idx=0003) are stripped with no emitted code;
 * the USA renderer paginates automatically via mMsg_request_main_cursol when the
 * text box fills, so all pages live in one buffer separated only by newlines.
 *
 * Japan font atlas: I4 GCN-tiled texture, 64 glyphs wide × 38 rows tall.
 *   Width  = 64 × 12 = 768 px
 *   Height = 38 × 16 = 608 px
 *   Size   = 768 × 608 / 2 = 233,472 bytes
 *   Slots 0-255: char map glyphs (byte value = slot index)
 *   Slots 256+:  reserved for future kanji bank entries
 */
#ifdef TARGET_PC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "pc_disc.h"

/* --- Atlas geometry (must match gen_japan_font.py) --- */
#define JPN_GLYPH_W    12
#define JPN_GLYPH_H    16
#define JPN_ATLAS_COLS 64
#define JPN_ATLAS_ROWS 38
#define JPN_ATLAS_W    (JPN_ATLAS_COLS * JPN_GLYPH_W)   /* 768 */
#define JPN_ATLAS_H    (JPN_ATLAS_ROWS * JPN_GLYPH_H)   /* 608 */
#define JPN_ATLAS_SIZE (JPN_ATLAS_W * JPN_ATLAS_H / 2)  /* 233472 */
#define JPN_ATLAS_SLOTS (JPN_ATLAS_COLS * JPN_ATLAS_ROWS) /* 2432 */

/* Lead byte 0xFE for the internal Japan glyph encoding. */
#define JPN_GLYPH_LEAD 0xFE

/* Exposed globals (read by m_font_main.c_inc via extern) */
int g_japan_msg_active   = 0;
u8  g_japan_font_atlas[JPN_ATLAS_SIZE];  /* zero until atlas binary is loaded */

static u8*  g_japan_msg_buf       = NULL;
static u8*  g_japan_inf1          = NULL;
static u32  g_japan_inf1_count    = 0;
static u32  g_japan_inf1_stride   = 4;
static u8*  g_japan_dat1          = NULL;
static u32  g_japan_dat1_size     = 0;

/* Control code sizes (total bytes including 0x7F + ID byte).
 * Matches m_font_main.c_inc mFont_cont_info_tbl and GCNParser.ControlCodeSizeTable.
 * Index = control code ID (0x00-0x7C). */
static const u8 japan_cont_size_tbl[0x7D] = {
    2, 2, 2, 3, 2, 5, 2, 2, 5, 5, 5, 5, 5, 2, 4, 4, /* 0x00-0x0F */
    4, 4, 4, 6, 8,10, 6, 8,10, 2, 2, 2, 2, 2, 2, 2, /* 0x10-0x1F */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x20-0x2F */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x30-0x3F */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x40-0x4F */
    6, 3, 3, 3, 3, 2, 4, 4, 3, 3, 3, 2, 2, 2, 2, 2, /* 0x50-0x5F */
    2, 2, 2, 6, 3, 3, 4, 3, 2, 2, 6, 2, 2, 3, 3, 3, /* 0x60-0x6F */
    3, 2, 2, 2, 2, 2, 2, 4, 4,12,14, 2, 3           /* 0x70-0x7C */
};

static u32 jbe32(const u8* p) {
    return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|(u32)p[3];
}
static u16 jbe16(const u8* p) {
    return ((u16)p[0]<<8)|(u16)p[1];
}

/* Find a file by name inside a RARC blob. Returns pointer into blob, or NULL. */
static const u8* rarc_find(const u8* data, u32 data_sz, const char* fname, u32* out_sz) {
    u32 hdr_size, data_offset, info_off;
    u32 num_nodes, node_offset, entry_offset, str_tbl_off;
    u32 node_base, entry_base, str_base, file_data_base;
    u32 n;

    if (data_sz < 8 || memcmp(data, "RARC", 4) != 0) return NULL;

    hdr_size     = jbe32(data + 8);
    data_offset  = jbe32(data + 12);
    info_off     = hdr_size;

    num_nodes    = jbe32(data + info_off + 0);
    node_offset  = jbe32(data + info_off + 4);
    entry_offset = jbe32(data + info_off + 12);
    str_tbl_off  = jbe32(data + info_off + 20);

    node_base      = info_off + node_offset;
    entry_base     = info_off + entry_offset;
    str_base       = info_off + str_tbl_off;
    file_data_base = hdr_size + data_offset;

    for (n = 0; n < num_nodes; n++) {
        const u8* node = data + node_base + n * 16;
        u16 num_files   = jbe16(node + 10);
        u32 first_entry = jbe32(node + 12);
        u32 f;
        for (f = 0; f < num_files; f++) {
            const u8* entry = data + entry_base + (first_entry + f) * 20;
            u32 name_off = jbe32(entry + 4) & 0xFFFFFF;
            u32 doff     = jbe32(entry + 8);
            u32 sz       = jbe32(entry + 12);
            const char* name = (const char*)(data + str_base + name_off);
            if (entry[4] & 2) continue; /* directory */
            if (strcmp(name, fname) == 0) {
                *out_sz = sz;
                return data + file_data_base + doff;
            }
        }
    }
    return NULL;
}

void pc_japan_msg_init(void) {
    char iso_path[512];
    u8*  arc_buf = NULL;
    u32  arc_sz  = 0;
    const u8* msg_ptr;
    u32  msg_sz  = 0;
    u8*  msg_copy = NULL;
    const u8* p;
    u32  section_count, s;

    if (!pc_disc_find_by_gameid("GAEJ", iso_path, (int)sizeof(iso_path))) {
        fprintf(stderr, "[PC/JpnMsg] Japan ISO (GAEJ) not found - Japanese text unavailable\n");
        return;
    }
    fprintf(stderr, "[PC/JpnMsg] Found Japan ISO: %s\n", iso_path);

    if (!pc_disc_load_from_iso(iso_path, "forest_msg.arc", &arc_buf, &arc_sz)) {
        fprintf(stderr, "[PC/JpnMsg] forest_msg.arc not found in Japan ISO\n");
        return;
    }

    msg_ptr = rarc_find(arc_buf, arc_sz, "msg.bin", &msg_sz);
    if (!msg_ptr) {
        fprintf(stderr, "[PC/JpnMsg] msg.bin not found in forest_msg.arc\n");
        free(arc_buf);
        return;
    }

    msg_copy = (u8*)malloc(msg_sz);
    if (!msg_copy) {
        fprintf(stderr, "[PC/JpnMsg] out of memory for msg.bin (%lu bytes)\n", (unsigned long)msg_sz);
        free(arc_buf);
        return;
    }
    memcpy(msg_copy, msg_ptr, msg_sz);
    free(arc_buf);

    if (msg_sz < 32 || memcmp(msg_copy, "MESGbmg1", 8) != 0) {
        fprintf(stderr, "[PC/JpnMsg] msg.bin: unexpected format (not MESGbmg1)\n");
        free(msg_copy);
        return;
    }

    section_count = jbe32(msg_copy + 12);
    p = msg_copy + 32;

    for (s = 0; s < section_count && (u32)(p - msg_copy) + 8 <= msg_sz; s++) {
        u32 tag    = jbe32(p);
        u32 sec_sz = jbe32(p + 4);
        if (sec_sz < 8) break;

        if (tag == 0x494E4631u) { /* 'INF1' */
            g_japan_inf1_count  = jbe16(p + 8);
            g_japan_inf1_stride = jbe16(p + 10);
            if (g_japan_inf1_stride < 4) g_japan_inf1_stride = 4;
            g_japan_inf1        = (u8*)(p + 16);
        } else if (tag == 0x44415431u) { /* 'DAT1' */
            g_japan_dat1      = (u8*)(p + 8);
            g_japan_dat1_size = sec_sz - 8;
        }
        p += sec_sz;
    }

    if (!g_japan_inf1 || !g_japan_dat1) {
        fprintf(stderr, "[PC/JpnMsg] msg.bin: INF1/DAT1 sections not found\n");
        free(msg_copy);
        return;
    }

    g_japan_msg_buf = msg_copy;
    fprintf(stderr, "[PC/JpnMsg] Ready: %lu messages, DAT1=%lu bytes\n",
            (unsigned long)g_japan_inf1_count,
            (unsigned long)g_japan_dat1_size);

    /* Load the font atlas (prebuilt by gen_japan_font.py) */
    {
        FILE* f;
        char atlas_path[512];
        size_t n;

        snprintf(atlas_path, sizeof(atlas_path), "%s", iso_path);
        {
            char* slash  = strrchr(atlas_path, '/');
            char* bslash = strrchr(atlas_path, '\\');
            char* sep    = (slash > bslash) ? slash : bslash;
            if (sep) strcpy(sep + 1, "japan_font_atlas.bin");
            else     strcpy(atlas_path, "japan_font_atlas.bin");
        }

        f = fopen(atlas_path, "rb");
        if (!f) {
            fprintf(stderr, "[PC/JpnMsg] Atlas not found at '%s'; glyphs will be blank\n"
                            "[PC/JpnMsg] Run pc/tools/gen_japan_font.py to generate it\n",
                    atlas_path);
        } else {
            n = fread(g_japan_font_atlas, 1, JPN_ATLAS_SIZE, f);
            fclose(f);
            if (n != JPN_ATLAS_SIZE) {
                fprintf(stderr, "[PC/JpnMsg] Atlas truncated (%zu/%u bytes); clearing\n",
                        n, JPN_ATLAS_SIZE);
                memset(g_japan_font_atlas, 0, JPN_ATLAS_SIZE);
            } else {
                fprintf(stderr, "[PC/JpnMsg] Loaded atlas: %u bytes from '%s'\n",
                        JPN_ATLAS_SIZE, atlas_path);
            }
        }
    }

    g_japan_msg_active = 1;
    fprintf(stderr, "[PC/JpnMsg] Active - Japanese text overlay ready\n");
}

/* Copy Japan message idx into dst, transcoding from DnMe+ custom encoding to
 * internal format. Returns 1 and sets *out_size on success; 0 if unavailable
 * or if the message contained no displayable glyphs (so USA text shows instead). */
int pc_japan_msg_copy(int idx, u8* dst, u32 dst_max, u32* out_size) {
    const u8* src;
    const u8* src_end;
    u8* out;
    u8* out_end;
    u32 start, end;
    int glyph_count = 0;

    if (!g_japan_inf1 || !g_japan_dat1) return 0;

    if (idx < 0 || (u32)idx >= g_japan_inf1_count) return 0;

    start = jbe32(g_japan_inf1 + (u32)idx * g_japan_inf1_stride);
    end   = ((u32)idx + 1 < g_japan_inf1_count)
          ? jbe32(g_japan_inf1 + ((u32)idx + 1) * g_japan_inf1_stride)
          : g_japan_dat1_size;

    if (end <= start || start >= g_japan_dat1_size) return 0;
    if (end > g_japan_dat1_size) end = g_japan_dat1_size;

    src     = g_japan_dat1 + start;
    src_end = g_japan_dat1 + end;
    out     = dst;
    out_end = dst + dst_max - 3; /* leave room for 0x7F 0x00 terminator + null */

    while (src < src_end && out < out_end) {
        u8 c = *src++;

        if (c == 0x80) {
            /* MessageTag: SIZE byte = total tag length including the 0x80 byte itself.
             * Format: 0x80 SIZE GROUP INDEX_HI INDEX_LO DATA[SIZE-5]
             * GROUP=0x01 INDEX_LO=0x03 (tag: 80 05 01 00 03) = page break.
             * All other tags are runtime substitution tokens - strip silently. */
            if (src < src_end) {
                u8 size = *src++;
                u32 remaining = (size >= 2) ? (u32)(size - 2) : 0;
                if (remaining >= 3 && src + remaining <= src_end
                        && src[0] == 0x01 && src[2] == 0x03) {
                    /* Japan page break [grp=01 idx=0003]: strip the tag and any
                     * trailing newline. The USA renderer paginates automatically -
                     * when the text box fills it pauses for A, then
                     * mMsg_request_main_cursol scrolls to the next page.
                     * No explicit control code is needed between pages. */
                    while (out > dst && *(out - 1) == 0xCD) out--;
                    src += remaining;
                } else {
                    if (src + remaining <= src_end)
                        src += remaining;
                    else
                        src = src_end;
                }
            }
        } else if (c == 0x7F) {
            /* Control code: ID byte, then params.
             * Total size (including 0x7F + ID) given by japan_cont_size_tbl[ID]. */
            if (src < src_end) {
                u8 id = *src++;
                u8 total = (id < 0x7Du) ? japan_cont_size_tbl[id] : 2u;
                u32 params = (total >= 2) ? (u32)(total - 2) : 0;
                if (src + params <= src_end)
                    src += params;
                else
                    src = src_end;
            }
        } else if (c == 0xCD) {
            /* Newline - pass through */
            *out++ = 0xCD;
        } else if (c == 0x20) {
            /* Space - pass through */
            *out++ = 0x20;
        } else {
            /* Regular character: slot index = byte value (0x00-0xFF).
             * Emit 3-byte internal glyph token: JPN_GLYPH_LEAD 0x00 c */
            if (out + 3 <= out_end) {
                *out++ = JPN_GLYPH_LEAD;
                *out++ = 0x00;
                *out++ = c;
                glyph_count++;
            }
        }
    }

    /* If no glyphs were produced (message was all control codes / substitution tokens),
     * return 0 so the caller falls through to USA text. */
    if (glyph_count == 0) return 0;

    /* Strip trailing newlines left over before the page-break tag */
    while (out > dst && *(out - 1) == 0xCD)
        out--;

    /* Append USA-format end code so mMsg_Count_MsgData terminates correctly */
    *out++ = 0x7F; /* CHAR_CONTROL_CODE */
    *out++ = 0x00; /* mFont_CONT_CODE_LAST */
    *out   = 0;
    *out_size = (u32)(out - dst);
    return 1;
}

#endif /* TARGET_PC */
