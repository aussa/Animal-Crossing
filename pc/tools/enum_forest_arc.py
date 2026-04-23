#!/usr/bin/env python3
"""Enumerate all files inside forest_1st.arc (and optionally forest_2nd.arc)
from the Japan ISO. Prints every filename and compares against the USA
resource name table in jsyswrap.cpp to find matches/mismatches.

Usage:
  python3 pc/tools/enum_forest_arc.py [--arc forest_1st.arc|forest_2nd.arc]
"""

import struct, sys, os
from pathlib import Path

REPO     = Path(__file__).resolve().parent.parent.parent
ISO_PATH = REPO / "pc/build32/bin/rom/Animal Crossing (Japan).iso"

# Resource names from src/static/jsyswrap.cpp (aram_resName[]), in enum order.
# Index = resource_index enum value.
USA_RES_NAMES = [
    "fgdata.bin",          # 0  RESOURCE_FGDATA
    "mail_data.bin",       # 1  RESOURCE_MAIL
    "mail_data_table.bin", # 2  RESOURCE_MAIL_TABLE
    "maila_data.bin",      # 3  RESOURCE_MAILA
    "maila_data_table.bin",# 4  RESOURCE_MAILA_TABLE
    "mailb_data.bin",      # 5  RESOURCE_MAILB
    "mailb_data_table.bin",# 6  RESOURCE_MAILB_TABLE
    "mailc_data.bin",      # 7  RESOURCE_MAILC
    "mailc_data_table.bin",# 8  RESOURCE_MAILC_TABLE
    "pallet_boy.bin",      # 9  RESOURCE_PALLET_BOY
    "ps_data.bin",         # 10 RESOURCE_PS
    "ps_data_table.bin",   # 11 RESOURCE_PS_TABLE
    "psz_data.bin",        # 12 RESOURCE_PSZ
    "psz_data_table.bin",  # 13 RESOURCE_PSZ_TABLE
    "select_data.bin",     # 14 RESOURCE_SELECT
    "select_data_table.bin",# 15 RESOURCE_SELECT_TABLE
    "string_data.bin",     # 16 RESOURCE_STRING
    "string_data_table.bin",# 17 RESOURCE_STRING_TABLE
    "superz_data.bin",     # 18 RESOURCE_SUPERZ
    "superz_data_table.bin",# 19 RESOURCE_SUPERZ_TABLE
    "super_data.bin",      # 20 RESOURCE_SUPER
    "super_data_table.bin",# 21 RESOURCE_SUPER_TABLE
    "tex_boy.bin",         # 22 RESOURCE_TEX_BOY
    "face_boy.bin",        # 23 RESOURCE_FACE_BOY
    # --- forest_arc_aram2_p (forest_2nd.arc) starts at index 24 ---
    "fgnpcdata.bin",       # 24 RESOURCE_FGNPCDATA
    "message_data.bin",    # 25 RESOURCE_MESSAGE
    "message_data_table.bin",# 26 RESOURCE_MESSAGE_TABLE
    "my_original.bin",     # 27 RESOURCE_MY_ORIGINAL
    "needlework.bin",      # 28 RESOURCE_NEEDLEWORK_JOYBOOT
    "player_room_floor.bin",# 29 RESOURCE_PLAYER_ROOM_FLOOR
    "player_room_wall.bin",# 30 RESOURCE_PLAYER_ROOM_WALL
    "npc_name_str_table.bin",# 31 RESOURCE_NPC_NAME_STR_TABLE
    "d_obj_npc_stock_sch.bin",# 32 RESOURCE_D_OBJ_NPC_STOCK_SCH
    "d_obj_npc_stock_scl.bin",# 33 RESOURCE_D_OBJ_NPC_STOCK_SCL
    "title.bti",           # 34 RESOURCE_TITLE
    "mura_spring.bti",     # 35
    "mura_summer.bti",     # 36
    "mura_fall.bti",       # 37
    "mura_winter.bti",     # 38
    "odekake.bti",         # 39
    "omake.bti",           # 40
    "eki1.bti",            # 41
    "eki1_2.bti",          # 42
    "eki1_3.bti",          # 43
    "eki1_4.bti",          # 44
    "eki1_5.bti",          # 45
    "eki2.bti",            # 46
    "eki2_2.bti",          # 47
    "eki2_3.bti",          # 48
    "eki2_4.bti",          # 49
    "eki2_5.bti",          # 50
    "eki3.bti",            # 51
    "eki3_2.bti",          # 52
    "eki3_3.bti",          # 53
    "eki3_4.bti",          # 54
    "eki3_5.bti",          # 55
    "tegami.bti",          # 56
    "tegami2.bti",         # 57
    "famikon.bti",         # 58
    "boy1.bti",            # 59
    "boy2.bti",            # 60
    "boy3.bti",            # 61
    "boy4.bti",            # 62
    "boy5.bti",            # 63
    "boy6.bti",            # 64
    "boy7.bti",            # 65
    "boy8.bti",            # 66
    "girl1.bti",           # 67
    "girl2.bti",           # 68
    "girl3.bti",           # 69
    "girl4.bti",           # 70
    "girl5.bti",           # 71
    "girl6.bti",           # 72
    "girl7.bti",           # 73
    "girl8.bti",           # 74
    "d_bg_island_sch.bin", # 75
]
USA_RES_SET = set(USA_RES_NAMES)

def be16(d, o): return struct.unpack_from('>H', d, o)[0]
def be32(d, o): return struct.unpack_from('>I', d, o)[0]

def read_at(fp, offset, size):
    fp.seek(offset)
    return fp.read(size)

def parse_fst(fp, fst_off):
    hdr = read_at(fp, fst_off, 12)
    num_ent = be32(hdr, 8)
    str_tbl_off = fst_off + num_ent * 12
    entries = read_at(fp, fst_off, num_ent * 12)
    str_tbl = read_at(fp, str_tbl_off, 0x10000)

    def get_str(off):
        end = str_tbl.index(b'\x00', off)
        return str_tbl[off:end].decode('ascii', errors='replace')

    files = {}
    dir_stack = [('', 0)]
    i = 0
    while i < num_ent:
        e = entries[i*12:(i+1)*12]
        is_dir   = e[0] != 0
        name_off = be32(e, 0) & 0xFFFFFF
        name     = get_str(name_off)
        if is_dir:
            if i == 0:
                parent_idx = be32(e, 4)
                next_idx   = be32(e, 8)
            else:
                parent_idx = be32(e, 4)
                next_idx   = be32(e, 8)
                path = dir_stack[-1][0] + name + '/'
                dir_stack.append((path, next_idx))
        else:
            disc_off = be32(e, 4)
            size     = be32(e, 8)
            path = dir_stack[-1][0] + name
            if name not in ('.', '..'):
                files[path] = (disc_off, size)
        i += 1
        while len(dir_stack) > 1 and i >= dir_stack[-1][1]:
            dir_stack.pop()
    return files

def parse_rarc(data):
    """Parse a RARC (JKRArchive) blob. Returns list of (path, offset_in_blob, size)."""
    if data[:4] != b'RARC':
        raise ValueError(f"Not a RARC: magic={data[:4].hex()}")

    # RARC header (32 bytes)
    file_size   = be32(data, 4)
    hdr_size    = be32(data, 8)   # size of header block (usually 32)
    data_offset = be32(data, 12)  # offset to file data, relative to end of header (hdr_size)
    # data_size = be32(data, 16)

    # Info block starts at hdr_size
    info_off    = hdr_size
    num_nodes   = be32(data, info_off + 0)
    node_offset = be32(data, info_off + 4)   # relative to info_off
    num_entries = be32(data, info_off + 8)
    entry_offset= be32(data, info_off + 12)  # relative to info_off
    str_tbl_size= be32(data, info_off + 16)
    str_tbl_off = be32(data, info_off + 20)  # relative to info_off

    node_base  = info_off + node_offset
    entry_base = info_off + entry_offset
    str_base   = info_off + str_tbl_off
    file_data_base = hdr_size + data_offset

    def get_str(off):
        end = data.index(b'\x00', str_base + off)
        return data[str_base + off:end].decode('shift-jis', errors='replace')

    files = []
    for n in range(num_nodes):
        node = data[node_base + n*16 : node_base + n*16 + 16]
        node_type   = node[0:4].decode('ascii', errors='replace').rstrip('\x00')
        name_off    = be32(node, 4)
        name_hash   = be16(node, 8)
        num_files   = be16(node, 10)
        first_entry = be32(node, 12)

        node_name = get_str(name_off)

        for f in range(num_files):
            eidx  = first_entry + f
            entry = data[entry_base + eidx*20 : entry_base + eidx*20 + 20]
            file_id    = be16(entry, 0)
            file_hash  = be16(entry, 2)
            flags      = entry[4]
            fname_off  = be16(entry, 6)  # actually 3 bytes in some versions; be16 is fine for name offset
            fname_off  = be32(entry, 4) & 0xFFFFFF  # name offset is 24-bit in the flag+nameoff field
            data_off   = be32(entry, 8)
            size       = be32(entry, 12)

            is_dir = (flags & 0x02) != 0
            if is_dir:
                continue

            fname = get_str(fname_off)
            if fname in ('.', '..'):
                continue

            abs_data_off = file_data_base + data_off
            files.append((node_name + '/' + fname, fname, abs_data_off, size))

    return files


def find_file_in_iso(fp, filename):
    """Find a file in the GCM/ISO filesystem and return its (disc_offset, size)."""
    header = read_at(fp, 0, 0x460)
    fst_off  = be32(header, 0x424)
    fst_size = be32(header, 0x428)

    fst = read_at(fp, fst_off, fst_size)
    num_root = be32(fst, 8)
    str_base = num_root * 12

    def get_str(off):
        end = fst.index(b'\x00', str_base + off)
        return fst[str_base + off:end].decode('ascii', errors='replace')

    i = 1
    dir_end = [num_root]
    while i < num_root:
        e = fst[i*12:(i+1)*12]
        is_dir = e[0] != 0
        name_off = be32(e, 0) & 0xFFFFFF
        name = get_str(name_off)
        if is_dir:
            dir_end.append(be32(e, 8))
        else:
            disc_off = be32(e, 4)
            size     = be32(e, 8)
            if name == filename:
                return disc_off, size
        i += 1
    return None, None


def main():
    arc_name = "forest_1st.arc"
    for arg in sys.argv[1:]:
        if arg.startswith('--arc'):
            arc_name = arg.split('=', 1)[-1] if '=' in arg else sys.argv[sys.argv.index(arg)+1]

    print(f"ISO: {ISO_PATH}")
    print(f"Looking for: {arc_name}")

    with open(ISO_PATH, 'rb') as fp:
        disc_off, size = find_file_in_iso(fp, arc_name)
        if disc_off is None:
            print(f"ERROR: {arc_name} not found in ISO filesystem")
            sys.exit(1)
        print(f"Found at disc offset 0x{disc_off:08X}, size {size:,} bytes")
        data = read_at(fp, disc_off, size)

    files = parse_rarc(data)

    print(f"\n=== Files in {arc_name} ({len(files)} files) ===")
    japan_names = set()
    for (path, fname, off, sz) in sorted(files, key=lambda x: x[1]):
        matched = fname in USA_RES_SET
        tag = "  [MATCH]" if matched else ""
        print(f"  {fname:40s}  size={sz:8,}  off=+0x{off:07X}{tag}")
        japan_names.add(fname)

    print(f"\n=== USA resource names NOT found in Japan {arc_name} ===")
    arc_idx_start = 0
    arc_idx_end   = 24  # first 24 are forest_1st (indices 0-23)
    if '2nd' in arc_name:
        arc_idx_start = 24
        arc_idx_end   = len(USA_RES_NAMES)

    missing = []
    for i in range(arc_idx_start, arc_idx_end):
        n = USA_RES_NAMES[i]
        if n not in japan_names:
            missing.append((i, n))

    if missing:
        for (i, n) in missing:
            print(f"  [{i:2d}] {n}")
    else:
        print("  (all USA names found in Japan archive)")

    print(f"\n=== Japan {arc_name} names NOT in USA resource table ===")
    extra = [fname for fname in japan_names if fname not in USA_RES_SET]
    if extra:
        for n in sorted(extra):
            print(f"  {n}")
    else:
        print("  (no extra names)")


if __name__ == '__main__':
    main()
