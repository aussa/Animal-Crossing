#!/usr/bin/env python3
"""Generate the Japan font atlas (japan_font_atlas.bin) for pc_japan_msg.c.

The atlas is a GCN I4-format tiled texture:
  - 64 glyphs per row × 38 rows = 2,432 glyph slots
  - Each glyph: 12 × 16 pixels
  - Atlas: 768 × 608 pixels, I4 (4-bit intensity), GCN 8×8-tile layout
  - Total: 768 × 608 / 2 = 233,472 bytes

Japan BMG encoding (Doubutsu no Mori e+) uses a custom single-byte character
map (NOT Shift-JIS). Each byte 0x00-0xFF maps to a Unicode character. The atlas
slot index equals the byte value directly (slot 0 = byte 0x00 = あ, etc.).

Slots 0-255 cover all 256 char map entries.
Slots 256+ are reserved for future kanji bank extensions (not yet used).

Requires: pip install Pillow
A CJK-capable TTF/OTF; pass --font <path.ttf> or let the script find one.

Usage:
  python gen_japan_font.py [--font path/to/font.ttf] [--out path/to/japan_font_atlas.bin]
"""
import argparse
import sys
from pathlib import Path

# ── Atlas constants (must match pc_japan_msg.c) ──────────────────────────────
GLYPH_W     = 12
GLYPH_H     = 16
ATLAS_COLS  = 64
ATLAS_ROWS  = 38
ATLAS_W     = ATLAS_COLS * GLYPH_W   # 768
ATLAS_H     = ATLAS_ROWS * GLYPH_H   # 608
ATLAS_SIZE  = ATLAS_W * ATLAS_H // 2 # 233472
ATLAS_SLOTS = ATLAS_COLS * ATLAS_ROWS # 2432

REPO = Path(__file__).resolve().parent.parent.parent
ISO_PATH = REPO / "pc/build32/bin/rom/Animal Crossing (Japan).iso"

# ── Doubutsu no Mori e+ character map ────────────────────────────────────────
# Source: Animal-Crossing-Text-Editor/Classes/TextUtility.cs
# Doubutsu_no_Mori_Plus_Character_Map
# Each entry: byte_value -> Unicode string to render.
# 0x7F (control code prefix) and 0x80 (MessageTag) are not rendered (blank slot).
# 0xCD (newline) and 0x20 (space) are passed through by the transcoder, not rendered.
DNME_CHAR_MAP = {
    0x00: "あ", 0x01: "い", 0x02: "う", 0x03: "え", 0x04: "お",
    0x05: "か", 0x06: "き", 0x07: "く", 0x08: "け", 0x09: "こ",
    0x0A: "さ", 0x0B: "し", 0x0C: "す", 0x0D: "せ", 0x0E: "そ",
    0x0F: "た", 0x10: "ち", 0x11: "つ", 0x12: "て", 0x13: "と",
    0x14: "な", 0x15: "に", 0x16: "ぬ", 0x17: "ね", 0x18: "の",
    0x19: "は", 0x1A: "ひ", 0x1B: "ふ", 0x1C: "へ", 0x1D: "ほ",
    0x1E: "ま", 0x1F: "み",
    0x20: " ",  0x21: "!", 0x22: '"',  0x23: "む", 0x24: "め",
    0x25: "%",  0x26: "&", 0x27: "'",  0x28: "(",  0x29: ")",
    0x2A: "~",  0x2B: "♥", 0x2C: ",",  0x2D: "-",  0x2E: ".",
    0x2F: "♪",
    0x30: "0",  0x31: "1", 0x32: "2",  0x33: "3",  0x34: "4",
    0x35: "5",  0x36: "6", 0x37: "7",  0x38: "8",  0x39: "9",
    0x3A: ":",  0x3B: "•", 0x3C: "<",  0x3D: "+",  0x3E: ">",
    0x3F: "?",
    0x40: "@",
    0x41: "A",  0x42: "B", 0x43: "C",  0x44: "D",  0x45: "E",
    0x46: "F",  0x47: "G", 0x48: "H",  0x49: "I",  0x4A: "J",
    0x4B: "K",  0x4C: "L", 0x4D: "M",  0x4E: "N",  0x4F: "O",
    0x50: "P",  0x51: "Q", 0x52: "R",  0x53: "S",  0x54: "T",
    0x55: "U",  0x56: "V", 0x57: "W",  0x58: "X",  0x59: "Y",
    0x5A: "Z",
    0x5B: "も", 0x5C: "×", 0x5D: "や", 0x5E: "ゆ", 0x5F: "_",
    0x60: "よ",
    0x61: "a",  0x62: "b", 0x63: "c",  0x64: "d",  0x65: "e",
    0x66: "f",  0x67: "g", 0x68: "h",  0x69: "i",  0x6A: "j",
    0x6B: "k",  0x6C: "l", 0x6D: "m",  0x6E: "n",  0x6F: "o",
    0x70: "p",  0x71: "q", 0x72: "r",  0x73: "s",  0x74: "t",
    0x75: "u",  0x76: "v", 0x77: "w",  0x78: "x",  0x79: "y",
    0x7A: "z",
    0x7B: "ら", 0x7C: "り", 0x7D: "る", 0x7E: "れ",
    0x7F: None,  # control code prefix — blank slot
    0x80: None,  # MessageTag marker — blank slot
    0x81: "。", 0x82: "｢", 0x83: "｣", 0x84: "、", 0x85: "･",
    0x86: "ヲ", 0x87: "ァ", 0x88: "ィ", 0x89: "ゥ", 0x8A: "ェ",
    0x8B: "ォ", 0x8C: "ャ", 0x8D: "ュ", 0x8E: "ョ", 0x8F: "ッ",
    0x90: "ー", 0x91: "ア", 0x92: "イ", 0x93: "ウ", 0x94: "エ",
    0x95: "オ", 0x96: "カ", 0x97: "キ", 0x98: "ク", 0x99: "ケ",
    0x9A: "コ", 0x9B: "サ", 0x9C: "シ", 0x9D: "ス", 0x9E: "セ",
    0x9F: "ソ", 0xA0: "タ", 0xA1: "チ", 0xA2: "ツ", 0xA3: "テ",
    0xA4: "ト", 0xA5: "ナ", 0xA6: "ニ", 0xA7: "ヌ", 0xA8: "ネ",
    0xA9: "ノ", 0xAA: "ハ", 0xAB: "ヒ", 0xAC: "フ", 0xAD: "ヘ",
    0xAE: "ホ", 0xAF: "マ", 0xB0: "ミ", 0xB1: "ム", 0xB2: "メ",
    0xB3: "モ", 0xB4: "ヤ", 0xB5: "ユ", 0xB6: "ヨ", 0xB7: "ラ",
    0xB8: "リ", 0xB9: "ル", 0xBA: "レ", 0xBB: "ロ", 0xBC: "ワ",
    0xBD: "ン", 0xBE: "ヴ", 0xBF: "☺",
    0xC0: "ろ", 0xC1: "わ", 0xC2: "を", 0xC3: "ん", 0xC4: "ぁ",
    0xC5: "ぃ", 0xC6: "ぅ", 0xC7: "ぇ", 0xC8: "ぉ", 0xC9: "ゃ",
    0xCA: "ゅ", 0xCB: "ょ", 0xCC: "っ",
    0xCD: None,  # newline — blank slot (renderer handles 0xCD directly)
    0xCE: "ガ", 0xCF: "ギ", 0xD0: "グ", 0xD1: "ゲ", 0xD2: "ゴ",
    0xD3: "ザ", 0xD4: "ジ", 0xD5: "ズ", 0xD6: "ゼ", 0xD7: "ゾ",
    0xD8: "ダ", 0xD9: "ヂ", 0xDA: "ヅ", 0xDB: "デ", 0xDC: "ド",
    0xDD: "バ", 0xDE: "ビ", 0xDF: "ブ", 0xE0: "ベ", 0xE1: "ボ",
    0xE2: "パ", 0xE3: "ピ", 0xE4: "プ", 0xE5: "ペ", 0xE6: "ポ",
    0xE7: "が", 0xE8: "ぎ", 0xE9: "ぐ", 0xEA: "げ", 0xEB: "ご",
    0xEC: "ざ", 0xED: "じ", 0xEE: "ず", 0xEF: "ぜ", 0xF0: "ぞ",
    0xF1: "だ", 0xF2: "ぢ", 0xF3: "づ", 0xF4: "で", 0xF5: "ど",
    0xF6: "ば", 0xF7: "び", 0xF8: "ぶ", 0xF9: "べ", 0xFA: "ぼ",
    0xFB: "ぱ", 0xFC: "ぴ", 0xFD: "ぷ", 0xFE: "ぺ", 0xFF: "ぽ",
}

# ── Font candidates ───────────────────────────────────────────────────────────
CANDIDATE_FONTS = [
    "C:/Windows/Fonts/msgothic.ttc",
    "C:/Windows/Fonts/meiryo.ttc",
    "C:/Windows/Fonts/YuGothM.ttc",
    "C:/Windows/Fonts/NotoSansCJKjp-Regular.otf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
    "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
    "/Library/Fonts/Osaka.ttf",
]

def find_font():
    for p in CANDIDATE_FONTS:
        if Path(p).exists():
            return p
    return None

# ── Glyph rendering ───────────────────────────────────────────────────────────
def render_glyph_i4(font, char_str, glyph_w, glyph_h):
    """Render a single character into a glyph_w×glyph_h grayscale image,
    returned as a flat list of 4-bit intensity values (0-15), row-major."""
    from PIL import Image, ImageDraw, ImageFont

    img = Image.new('L', (glyph_w, glyph_h), 0)
    if not char_str:
        return [0] * (glyph_w * glyph_h)

    draw = ImageDraw.Draw(img)
    size = glyph_h
    try:
        fnt = ImageFont.truetype(font, size)
        bbox = fnt.getbbox(char_str)
        char_h = bbox[3] - bbox[1]
        if char_h > glyph_h:
            size = int(size * glyph_h / char_h)
            fnt = ImageFont.truetype(font, size)
            bbox = fnt.getbbox(char_str)
        bw = bbox[2] - bbox[0]
        bh = bbox[3] - bbox[1]
        x = (glyph_w - bw) // 2 - bbox[0]
        y = (glyph_h - bh) // 2 - bbox[1]
        draw.text((x, y), char_str, font=fnt, fill=255)
    except Exception:
        return [0] * (glyph_w * glyph_h)

    pixels = list(img.tobytes())
    return [p >> 4 for p in pixels]

# ── Atlas packing ─────────────────────────────────────────────────────────────
def pack_gcn_i4_tile(atlas_pixels, atlas_w, atlas_h):
    """Pack linear-order 4-bit pixels into GCN I4 8×8-tile format."""
    out = bytearray(atlas_w * atlas_h // 2)
    tiles_x = atlas_w // 8
    tiles_y = atlas_h // 8

    for ty in range(tiles_y):
        for tx in range(tiles_x):
            tile_base = (ty * tiles_x + tx) * 32
            for py in range(8):
                for px in range(8):
                    sx = tx * 8 + px
                    sy = ty * 8 + py
                    pixel_in_tile = py * 8 + px
                    byte_in_tile  = pixel_in_tile // 2
                    is_high       = (pixel_in_tile % 2 == 0)
                    nib = atlas_pixels[sy * atlas_w + sx] & 0xF
                    if is_high:
                        out[tile_base + byte_in_tile] |= nib << 4
                    else:
                        out[tile_base + byte_in_tile] |= nib

    return bytes(out)

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--font', help='Path to a CJK-capable TTF/OTF font file')
    ap.add_argument('--out',  default=None,
                    help='Output path (default: next to Japan ISO)')
    ap.add_argument('--iso',  default=str(ISO_PATH),
                    help='Path to Japan ISO (used only to determine output directory)')
    args = ap.parse_args()

    iso_path = Path(args.iso)
    out_path = Path(args.out) if args.out else iso_path.parent / "japan_font_atlas.bin"

    font_path = args.font or find_font()
    if not font_path:
        sys.exit("No CJK font found. Install a Japanese font or pass --font <path.ttf>")
    print(f"Using font: {font_path}")

    try:
        from PIL import ImageFont
        ImageFont.truetype(font_path, 1)
    except ImportError:
        sys.exit("Pillow not installed. Run: pip install Pillow")

    # Slot = byte value (0x00-0xFF). 256 slots needed, atlas has 2432.
    num_slots = 256
    print(f"Rendering {num_slots} glyphs (slot = byte value) into {ATLAS_W}x{ATLAS_H} atlas ...")
    atlas_pixels = [0] * (ATLAS_W * ATLAS_H)

    for slot in range(num_slots):
        char_str = DNME_CHAR_MAP.get(slot)
        # char_str is None for control/tag/newline bytes → leave blank
        if char_str is None:
            char_str = ""

        col = slot % ATLAS_COLS
        row = slot // ATLAS_COLS
        px0 = col * GLYPH_W
        py0 = row * GLYPH_H

        glyph_nibs = render_glyph_i4(font_path, char_str, GLYPH_W, GLYPH_H)
        for gy in range(GLYPH_H):
            for gx in range(GLYPH_W):
                atlas_pixels[(py0 + gy) * ATLAS_W + (px0 + gx)] = glyph_nibs[gy * GLYPH_W + gx]

        if (slot + 1) % 64 == 0 or slot + 1 == num_slots:
            print(f"  {slot+1}/{num_slots}", end='\r', flush=True)

    print()
    print("Packing into GCN I4 tiled format ...")
    atlas_bytes = pack_gcn_i4_tile(atlas_pixels, ATLAS_W, ATLAS_H)
    assert len(atlas_bytes) == ATLAS_SIZE, f"Size mismatch: {len(atlas_bytes)} != {ATLAS_SIZE}"

    out_path.write_bytes(atlas_bytes)
    print(f"Wrote {len(atlas_bytes):,} bytes -> {out_path}")

if __name__ == '__main__':
    main()
