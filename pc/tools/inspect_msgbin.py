#!/usr/bin/env python3
"""Inspect msg.bin from Japan's forest_msg.arc to understand its format."""
import struct, sys
from pathlib import Path

REPO     = Path(__file__).resolve().parent.parent.parent
ISO_PATH = REPO / "pc/build32/bin/rom/Animal Crossing (Japan).iso"

def be8(d,o):  return d[o]
def be16(d,o): return struct.unpack_from('>H',d,o)[0]
def be32(d,o): return struct.unpack_from('>I',d,o)[0]
def le32(d,o): return struct.unpack_from('<I',d,o)[0]

def read_at(fp,off,size):
    fp.seek(off); return fp.read(size)

def parse_fst_find(fp, filename):
    hdr = read_at(fp,0,0x460)
    fst_off  = be32(hdr,0x424)
    fst_size = be32(hdr,0x428)
    fst = read_at(fp,fst_off,fst_size)
    num_root = be32(fst,8)
    str_base = num_root*12
    def gs(off):
        end=fst.index(b'\x00',str_base+off)
        return fst[str_base+off:end].decode('ascii',errors='replace')
    i=1
    while i<num_root:
        e=fst[i*12:(i+1)*12]
        is_dir=e[0]!=0
        name_off=be32(e,0)&0xFFFFFF
        name=gs(name_off)
        if not is_dir and name==filename:
            return be32(e,4),be32(e,8)
        i+=1
    return None,None

def parse_rarc_find(data, fname):
    """Find a file by name inside a RARC blob. Returns (offset_in_blob, size)."""
    if data[:4]!=b'RARC': return None,None
    hdr_size    = be32(data,8)
    data_offset = be32(data,12)
    info_off    = hdr_size
    num_nodes   = be32(data,info_off+0)
    node_offset = be32(data,info_off+4)
    num_entries = be32(data,info_off+8)
    entry_offset= be32(data,info_off+12)
    str_tbl_off = be32(data,info_off+20)
    node_base   = info_off+node_offset
    entry_base  = info_off+entry_offset
    str_base    = info_off+str_tbl_off
    file_data_base = hdr_size+data_offset
    def gs(off):
        end=data.index(b'\x00',str_base+off)
        return data[str_base+off:end].decode('shift-jis',errors='replace')
    for n in range(num_nodes):
        node=data[node_base+n*16:node_base+n*16+16]
        num_files=be16(node,10)
        first_entry=be32(node,12)
        for f in range(num_files):
            eidx=first_entry+f
            entry=data[entry_base+eidx*20:entry_base+eidx*20+20]
            flags=entry[4]
            if flags&2: continue
            fname_off=be32(entry,4)&0xFFFFFF
            name=gs(fname_off)
            if name==fname:
                doff=be32(entry,8)
                sz=be32(entry,12)
                return file_data_base+doff, sz
    return None,None

with open(ISO_PATH,'rb') as fp:
    arc_off,arc_sz = parse_fst_find(fp,'forest_msg.arc')
    print(f"forest_msg.arc at 0x{arc_off:08X}, size {arc_sz:,}")
    arc_data = read_at(fp,arc_off,arc_sz)

msg_off, msg_sz = parse_rarc_find(arc_data,'msg.bin')
print(f"msg.bin at +0x{msg_off:07X} in arc blob, size {msg_sz:,} ({msg_sz/1024/1024:.2f} MB)")

msg = arc_data[msg_off:msg_off+msg_sz]

print(f"\n--- First 64 bytes (hex) ---")
for i in range(0,64,16):
    h=' '.join(f'{b:02x}' for b in msg[i:i+16])
    print(f"  +{i:04X}: {h}")

# Try to detect known message format headers
print(f"\n--- Format detection ---")
magic = msg[:4]
print(f"Magic: {magic.hex()} = '{magic.decode('ascii',errors='?')}'")

# MESG header (used in some AC versions)
if magic == b'MESG':
    print("Looks like MESG format")
    version = be32(msg,4)
    num_msg = be32(msg,8)
    print(f"  version=0x{version:08X} num_msg={num_msg}")

# JMessage format
if magic[:2] == b'JM' or magic[:4] == b'JMSG':
    print("Looks like JMessage format")

# Check if it looks like a simple offset table
first_u32 = be32(msg,0)
print(f"\nFirst u32 (BE): 0x{first_u32:08X} = {first_u32}")
print(f"First u32 (LE): 0x{le32(msg,0):08X} = {le32(msg,0)}")

# Look for ASCII/Shift-JIS text in first 512 bytes
print(f"\n--- Scanning first 512 bytes for strings ---")
i=0
strings_found=[]
while i<min(512,len(msg)):
    if 0x20<=msg[i]<0x7F or msg[i]>=0x81:
        j=i
        s=bytearray()
        while j<len(msg) and (0x20<=msg[j]<0x7F or msg[j]>=0x80):
            s.append(msg[j]); j+=1
        if len(s)>=4:
            try: txt=s.decode('shift-jis',errors='replace')
            except: txt=repr(s)
            strings_found.append((i,txt[:60]))
        i=j
    else:
        i+=1
for off,txt in strings_found[:20]:
    print(f"  +0x{off:04X}: {txt}")

# Try reading as an offset table: check if msg[0..3] is a count and msg[4..] are offsets
print(f"\n--- Possible offset table (first 20 u32s) ---")
for i in range(20):
    v = be32(msg,i*4)
    print(f"  [{i:2d}] +{i*4:04X}: 0x{v:08X} = {v}")

# Check around offset that first entry might point to
potential_first = be32(msg,4)
if 0 < potential_first < msg_sz:
    print(f"\n--- Data at offset pointed to by msg[4] = 0x{potential_first:X} ---")
    chunk = msg[potential_first:potential_first+32]
    print('  hex:', chunk.hex())
    try: print('  sjis:', chunk.decode('shift-jis',errors='replace'))
    except: pass
