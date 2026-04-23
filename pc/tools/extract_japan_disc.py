#!/usr/bin/env python3
"""Extract main.dol and foresta.rel.szs from the Japan Animal Crossing ISO.

Writes to:
  orig/GAFJ01_00/sys/main.dol
  orig/GAFJ01_00/files/foresta.rel.szs
"""

import struct, os, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
ISO_PATH = REPO / "pc/build32/bin/rom/Animal Crossing (Japan).iso"
OUT_SYS   = REPO / "orig/GAFJ01_00/sys"
OUT_FILES  = REPO / "orig/GAFJ01_00/files"

def be32(data, off): return struct.unpack_from('>I', data, off)[0]

def read_at(fp, offset, size):
    fp.seek(offset)
    return fp.read(size)

def calc_dol_size(hdr):
    max_end = 0
    for i in range(7):   # text sections
        off  = be32(hdr, i*4)
        sz   = be32(hdr, 0x90 + i*4)
        if off + sz > max_end: max_end = off + sz
    for i in range(11):  # data sections
        off  = be32(hdr, 0x1C + i*4)
        sz   = be32(hdr, 0xAC + i*4)
        if off + sz > max_end: max_end = off + sz
    return max_end

def parse_fst(fp, fst_off):
    """Return {path: (disc_offset, size)} for all files."""
    hdr = read_at(fp, fst_off, 12)
    num_ent = be32(hdr, 8)
    str_tbl = fst_off + num_ent * 12

    files = {}
    dir_stack = [{'next': num_ent, 'name': ''}]

    for i in range(1, num_ent):
        entry = read_at(fp, fst_off + i*12, 12)
        is_dir = entry[0]
        noff = (entry[1] << 16) | (entry[2] << 8) | entry[3]
        name_raw = read_at(fp, str_tbl + noff, 128)
        name = name_raw.split(b'\x00')[0].decode('latin-1')

        # pop dirs we've passed
        while len(dir_stack) > 1 and i >= dir_stack[-1]['next']:
            dir_stack.pop()

        if is_dir:
            next_entry = be32(entry, 8)
            dir_stack.append({'next': next_entry, 'name': name})
        else:
            disc_off = be32(entry, 4)
            size     = be32(entry, 8)
            path_parts = [d['name'] for d in dir_stack[1:]] + [name]
            full = '/'.join(path_parts)
            files[full] = (disc_off, size)

    return files

def main():
    if not ISO_PATH.exists():
        print(f"ERROR: ISO not found: {ISO_PATH}")
        sys.exit(1)

    OUT_SYS.mkdir(parents=True, exist_ok=True)
    OUT_FILES.mkdir(parents=True, exist_ok=True)

    with open(ISO_PATH, 'rb') as fp:
        # Verify GC magic
        magic = read_at(fp, 0x1C, 4)
        if magic != b'\xc2\x33\x9f\x3d':
            print("ERROR: Not a valid GC disc image")
            sys.exit(1)

        game_id = read_at(fp, 0, 6).decode('latin-1')
        print(f"Game ID: {game_id}")

        # Extract DOL
        dol_off = be32(read_at(fp, 0x420, 4), 0)
        dol_hdr = read_at(fp, dol_off, 0xE4)
        dol_size = calc_dol_size(dol_hdr)
        dol_data = read_at(fp, dol_off, dol_size)
        dol_out = OUT_SYS / "main.dol"
        dol_out.write_bytes(dol_data)
        print(f"DOL: {dol_size} bytes -> {dol_out}")

        # Parse FST
        fst_off = be32(read_at(fp, 0x424, 4), 0)
        files = parse_fst(fp, fst_off)

        # Find foresta.rel.szs (case-insensitive search)
        rel_key = None
        for k in files:
            if k.lower().endswith('foresta.rel.szs'):
                rel_key = k
                break

        if rel_key is None:
            print("ERROR: foresta.rel.szs not found in FST")
            print("Available files:", list(files.keys())[:20])
            sys.exit(1)

        rel_off, rel_size = files[rel_key]
        rel_data = read_at(fp, rel_off, rel_size)
        rel_out = OUT_FILES / "foresta.rel.szs"
        rel_out.write_bytes(rel_data)
        print(f"REL: {rel_size} bytes -> {rel_out}")
        print("Done.")

if __name__ == '__main__':
    main()
