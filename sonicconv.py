#!/usr/bin/env python3
#
# SonicArranger packed format converter
#
# This script should restore a binary SonicArranger module to its original
# format that can be loaded into SonicArranger again.
#
# by Thomas Meyer <mnemotron@gmail.com>
# Created: 2022-05-31
# Last change: 2022-06-02
#
# Based on:
# http://old.exotica.org.uk/tunes/formats/sonic/SONIC_AR.TXT
# https://github.com/Pyrdacor/SonicArranger/blob/main/Docs/PackedFileFormat.md
#
# This is not supposed to be an example of good, structured programming.
#

import sys
import struct

print("sonicconv -- SonicArranger packed format converter")
print("by Thomas Meyer\n")

if len(sys.argv) < 3:
    print("Usage: sonicconv.py <inputfile> <outputfile>")
    sys.exit()

in_filename = sys.argv[1]
out_filename = sys.argv[2]

# Read complete file. We're not handling gigabytes here.
try:
    with open(in_filename, "rb") as f:
        dat = f.read()
        f.close()
except FileNotFoundError:
    print(f"Cannot open file: {in_filename}")
    sys.exit()

dat_len = len(dat)
print(f"File size: 0x{dat_len:x}")

# Find beginning of the song data. We search for the song offset, which
# should be 0x00000028. Then we check the 32-bit value after that to be
# larger than 0x28, but less than 0x200. Should suffice.
offset = -1
for index in range(0, dat_len - 0x28, 2):
    (val1, val2) = struct.unpack_from(">LL", dat, index)
    if val1 == 0x28 and val2 > val1 and val2 < 0x200:
        offset = index
        break

if offset < 0:
    print("*** Error: SonicArranger module not found in file!")
    sys.exit(0)
else:
    print(f"Song found at offset: 0x{offset:x}")

# Get all data offsets
song_offset = offset + struct.unpack_from(">L", dat, offset)[0]
over_offset = offset + struct.unpack_from(">L", dat, offset + 4)[0]
note_offset = offset + struct.unpack_from(">L", dat, offset + 8)[0]
instr_offset = offset + struct.unpack_from(">L", dat, offset + 12)[0]
wave_offset = offset + struct.unpack_from(">L", dat, offset + 16)[0]
adsr_offset = offset + struct.unpack_from(">L", dat, offset + 20)[0]
amf_offset = offset + struct.unpack_from(">L", dat, offset + 24)[0]
sample_offset = offset + struct.unpack_from(">L", dat, offset + 28)[0]

# Calc section lengths
song_len = over_offset - song_offset
over_len = note_offset - over_offset
note_len = instr_offset - note_offset
instr_len = wave_offset - instr_offset
wave_len = adsr_offset - wave_offset
adsr_len = amf_offset - adsr_offset
amf_len = sample_offset - amf_offset

# Calc number of sections entries
song_cnt = song_len // 12
over_cnt = over_len // 16
note_cnt = note_len // 4
instr_cnt = instr_len // 152
wave_cnt = wave_len // 128
adsr_cnt = adsr_len // 128
amf_cnt = amf_len // 128
# Number of samples is in the file
sample_cnt = struct.unpack_from(">L", dat, sample_offset)[0]

# Build lists with information about the instruments.
# Instruments with samples are used to reconstruct the sample information later.
instr_names = []
sample_names = [bytes(30)] * sample_cnt
sample_lengths_from_instr = [0] * sample_cnt
sample_repeats_from_instr = [0] * sample_cnt
for instr_id in range(0, instr_cnt):
    i_offset = instr_offset + instr_id * 0x98
    instr_mode = struct.unpack_from(">H", dat, i_offset)[0]
    instr_sample_id = struct.unpack_from(">H", dat, i_offset + 2)[0]
    instr_sample_len = struct.unpack_from(">H", dat, i_offset + 4)[0]
    instr_sample_repeat = struct.unpack_from(">H", dat, i_offset + 6)[0]
    instr_name = dat[i_offset + 0x7a:i_offset + 0x7a + 30]
    instr_names.append(instr_name)
    # If this is a sampled instrument, store information at sample id offset
    if instr_mode == 0:
        sample_names[instr_sample_id] = instr_name
        sample_lengths_from_instr[instr_sample_id] = instr_sample_len * 2
        sample_repeats_from_instr[instr_sample_id] = instr_sample_repeat
    # Convert name to Python string
    instr_name = instr_name[:instr_name.index(b"\0")].decode("iso-8859-1")

# Handle samples
# + 0 long:    number of samples
# + 4 long []: sample length in bytes
# + xx:        sampledata
sample_len = 0
sample_lengths = []
for sample_length_offset in range(0, sample_cnt):
    this_sample_len = struct.unpack_from(">L", dat, sample_offset + 4 + sample_length_offset * 4)[0]
    sample_len += this_sample_len
    sample_lengths.append(this_sample_len)

sample_data_offset = sample_offset + 4 + sample_cnt * 4

print(f"song=  0x{song_offset:05x} len=0x{song_len:05x} cnt=0x{song_cnt:02x}")
print(f"over=  0x{over_offset:05x} len=0x{over_len:05x} cnt=0x{over_cnt:02x}")
print(f"note=  0x{note_offset:05x} len=0x{note_len:05x} cnt=0x{note_cnt:02x}")
print(f"instr= 0x{instr_offset:05x} len=0x{instr_len:05x} cnt=0x{instr_cnt:02x}")
print(f"wave=  0x{wave_offset:05x} len=0x{wave_len:05x} cnt=0x{wave_cnt:02x}")
print(f"adsr=  0x{adsr_offset:05x} len=0x{adsr_len:05x} cnt=0x{adsr_cnt:02x}")
print(f"amf=   0x{amf_offset:05x} len=0x{amf_len:05x} cnt=0x{amf_cnt:02x}")
print(f"sample=0x{sample_offset:05x} num=0x{sample_cnt:02x}")
print(f"sample_data=0x{sample_data_offset:05x} len=0x{sample_len:05x} end=0x{sample_data_offset + sample_len:05x}")

# Write the output file
with open(out_filename, "wb") as f:
    f.write("SOARV1.0".encode('ascii'))
    f.write("STBL".encode('ascii') + struct.pack(">L", song_cnt))
    f.write(dat[song_offset:over_offset])
    f.write("OVTB".encode('ascii') + struct.pack(">L", over_cnt))
    f.write(dat[over_offset:note_offset])
    f.write("NTBL".encode('ascii') + struct.pack(">L", note_cnt))
    f.write(dat[note_offset:instr_offset])
    f.write("INST".encode('ascii') + struct.pack(">L", instr_cnt))
    f.write(dat[instr_offset:wave_offset])
    f.write("SD8B".encode('ascii') + struct.pack(">L", sample_cnt))
    # Recreate sample info
    for i in range(0, sample_cnt):
        f.write(struct.pack(">L", sample_lengths_from_instr[i] // 2))
    for i in range(0, sample_cnt):
        f.write(struct.pack(">L", sample_repeats_from_instr[i]))
    for i in range(0, sample_cnt):
        f.write(sample_names[i])
    for i in range(0, sample_cnt):
        f.write(struct.pack(">L", sample_lengths[i]))
    f.write(dat[sample_data_offset:sample_data_offset + sample_len])
    f.write("SYWT".encode('ascii') + struct.pack(">L", wave_cnt))
    f.write(dat[wave_offset:adsr_offset])
    f.write("SYAR".encode('ascii') + struct.pack(">L", adsr_cnt))
    f.write(dat[adsr_offset:amf_offset])
    f.write("SYAF".encode('ascii') + struct.pack(">L", amf_cnt))
    f.write(dat[amf_offset:sample_offset])
    f.write("EDATV1.1".encode('ascii'))
    # Write some standard editor prefs. These are not in the binary source.
    f.write(bytes.fromhex("00 01 00 01 00 00 00 7b 00 00 00 00 00 01 00 03"))
    f.close()

print(f"Output written to: {out_filename}")
