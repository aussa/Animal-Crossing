#!/usr/bin/env python3
"""Dump and search Japan BMG messages from msg.bin as decoded text.

Usage:
  python dump_japan_msgs.py [--ids 2495-2515,10945-10960]
  python dump_japan_msgs.py [--search keyword]
  python dump_japan_msgs.py [--ids 2503] --raw
"""
import argparse, struct, sys, io
from pathlib import Path

# Force UTF-8 output so Japanese characters don't crash on Windows terminals
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

REPO = Path(__file__).resolve().parent.parent.parent
ISO_PATH = REPO / "pc/build32/bin/rom/Animal Crossing (Japan).iso"

def be32(d, o): return struct.unpack_from('>I', d, o)[0]
def be16(d, o): return struct.unpack_from('>H', d, o)[0]

def read_at(fp, off, size):
    fp.seek(off); return fp.read(size)

def fst_find(fp, filename):
    hdr = read_at(fp, 0, 0x460)
    fst_off = be32(hdr, 0x424); fst_sz = be32(hdr, 0x428)
    fst = read_at(fp, fst_off, fst_sz)
    num_root = be32(fst, 8); str_base = num_root * 12
    for i in range(1, num_root):
        e = fst[i*12:(i+1)*12]
        if e[0]: continue
        name_off = be32(e, 0) & 0xFFFFFF
        name = fst[str_base+name_off:].split(b'\x00',1)[0].decode('ascii','replace')
        if name == filename:
            return be32(e, 4), be32(e, 8)
    return None, None

def rarc_find(data, fname):
    if data[:4] != b'RARC': return None, None
    hdr_size = be32(data, 8); data_offset = be32(data, 12)
    info_off = hdr_size
    num_nodes = be32(data, info_off); node_offset = be32(data, info_off+4)
    entry_offset = be32(data, info_off+12); str_tbl_off = be32(data, info_off+20)
    node_base  = info_off + node_offset
    entry_base = info_off + entry_offset
    str_base   = info_off + str_tbl_off
    file_data_base = hdr_size + data_offset
    for n in range(num_nodes):
        nb = node_base + n*16
        num_files   = be16(data, nb+10)
        first_entry = be32(data, nb+12)
        for f in range(num_files):
            eb = entry_base + (first_entry+f)*20
            if data[eb+4] & 2: continue
            name_off = be32(data, eb+4) & 0xFFFFFF
            name = data[str_base+name_off:].split(b'\x00',1)[0].decode('shift-jis','replace')
            if name == fname:
                doff = be32(data, eb+8); sz = be32(data, eb+12)
                return file_data_base+doff, sz
    return None, None

def load_bmg(iso_path):
    with open(iso_path, 'rb') as fp:
        arc_off, arc_sz = fst_find(fp, 'forest_msg.arc')
        arc = read_at(fp, arc_off, arc_sz)
    msg_off, msg_sz = rarc_find(arc, 'msg.bin')
    msg = arc[msg_off:msg_off+msg_sz]
    assert msg[:8] == b'MESGbmg1', "Not a BMG file"
    p = 0x20
    inf1 = dat1 = None
    inf1_count = inf1_stride = 0
    for _ in range(be32(msg, 0x0C)):
        tag = msg[p:p+4]; sec_sz = be32(msg, p+4)
        if tag == b'INF1':
            inf1_count  = be16(msg, p+8)
            inf1_stride = be16(msg, p+10) or 4
            inf1 = msg[p+16:p+16+inf1_count*inf1_stride]
        elif tag == b'DAT1':
            dat1 = msg[p+8:p+sec_sz]
        p += sec_sz
    return inf1, inf1_count, inf1_stride, dat1

def get_raw(inf1, inf1_count, inf1_stride, dat1, idx):
    if idx < 0 or idx >= inf1_count: return None
    start = be32(inf1, idx * inf1_stride)
    end   = be32(inf1, (idx+1) * inf1_stride) if idx+1 < inf1_count else len(dat1)
    if end <= start or start >= len(dat1): return b''
    return dat1[start:min(end, len(dat1))]

def decode_msg(raw):
    """Decode raw Japan DAT1 bytes to a readable string."""
    i = 0; out = []
    while i < len(raw):
        c = raw[i]
        if c == 0x80:
            ln = raw[i+1] if i+1 < len(raw) and raw[i+1] >= 2 else 2
            out.append(f'<ctrl:{raw[i:i+ln].hex()}>')
            i += ln
        elif c == 0xCD:
            out.append('\n'); i += 1
        elif c == 0x20:
            out.append(' '); i += 1
        elif (0x81 <= c <= 0x9F or 0xE0 <= c <= 0xFC) and i+1 < len(raw):
            pair = bytes([c, raw[i+1]])
            decoded = None
            for codec in ('cp932', 'shift-jis', 'shift_jisx0213'):
                try: decoded = pair.decode(codec); break
                except: pass
            out.append(decoded if decoded else f'[{pair.hex()}]')
            i += 2
        elif 0x21 <= c < 0x7F:
            out.append(chr(c)); i += 1
        else:
            out.append(f'<{c:02x}>'); i += 1
    return ''.join(out)

def parse_ranges(spec):
    ids = []
    for part in spec.split(','):
        part = part.strip()
        if '-' in part:
            a, b = part.split('-', 1)
            ids.extend(range(int(a), int(b)+1))
        else:
            ids.append(int(part))
    return ids

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', default=str(ISO_PATH))
    ap.add_argument('--ids', default='2495-2515,10945-10960',
                    help='Comma-separated IDs or ranges (default shows K.K. and Rover areas)')
    ap.add_argument('--search', default=None,
                    help='Search all messages for a substring in decoded text')
    ap.add_argument('--raw', action='store_true', help='Print raw hex alongside decoded text')
    args = ap.parse_args()

    iso_path = Path(args.iso)
    if not iso_path.exists():
        sys.exit(f"ISO not found: {iso_path}")

    print(f"Loading msg.bin from {iso_path.name} ...", flush=True)
    inf1, inf1_count, inf1_stride, dat1 = load_bmg(str(iso_path))
    print(f"  {inf1_count} messages, stride={inf1_stride}, DAT1={len(dat1):,} bytes\n")

    if args.search:
        keyword = args.search
        print(f"Searching all {inf1_count} messages for {keyword!r} ...")
        hits = 0
        for idx in range(inf1_count):
            raw = get_raw(inf1, inf1_count, inf1_stride, dat1, idx)
            if raw is None: continue
            text = decode_msg(raw)
            if keyword in text:
                first_line = text.split('\n')[0][:100]
                print(f"[{idx:6d}] {first_line!r}")
                hits += 1
                if hits >= 100:
                    print("(stopped at 100 hits)"); break
        print(f"\n{hits} match(es) found.")
        return

    ids = parse_ranges(args.ids)
    for idx in ids:
        raw = get_raw(inf1, inf1_count, inf1_stride, dat1, idx)
        if raw is None:
            print(f"[{idx:6d}] OUT OF RANGE\n"); continue
        text = decode_msg(raw)
        if args.raw:
            print(f"[{idx:6d}] ({len(raw)}B) hex={raw[:32].hex()}")
        else:
            print(f"[{idx:6d}] ({len(raw)}B)")
        for line in (text.splitlines() or ['<empty>']):
            print(f"         {line}")
        print()

if __name__ == '__main__':
    main()
