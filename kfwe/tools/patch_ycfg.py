import pefile, struct, sys, capstone

SRC = "C:/Games/Warcraft 3/KFWE/bin/YDWEConfig.orig.exe"
OUT = sys.argv[1]
pe = pefile.PE(SRC)
data = bytearray(open(SRC, 'rb').read())
base = pe.OPTIONAL_HEADER.ImageBase
sec = {s.Name.rstrip(b'\0'): s for s in pe.sections}
def off(va): return pe.get_offset_from_rva(va - base)

HOOK, BACK, INSERT, ORIG_PUSH = 0x401ACD, 0x401AD2, 0x40CFCC, 0x405C42
STR = {'ThirdPartyPlugin': 0x41FB14, 'Option': 0x41F9B4, 'FeatureToggle': 0x41FB68}
assert data[off(HOOK):off(HOOK)+5] == b'\x68' + struct.pack('<I', ORIG_PUSH)
for s, va in STR.items():
    assert data[off(va):off(va)+len(s)+1] == s.encode() + b'\0', s

text, rdata = sec[b'.text'], sec[b'.rdata']
C = base + text.VirtualAddress + ((text.Misc_VirtualSize + 0xF) & ~0xF)
D = base + rdata.VirtualAddress + ((rdata.Misc_VirtualSize + 0xF) & ~0xF)
room_c = base + text.VirtualAddress + text.SizeOfRawData - C
room_d = base + rdata.VirtualAddress + rdata.SizeOfRawData - D
assert all(b == 0 for b in data[off(C):off(C)+room_c]) and all(b == 0 for b in data[off(D):off(D)+room_d])

OPTS = [
    ('EnableStormFastHeap', 0, 'ThirdPartyPlugin', 0),
    ('MapOptimizer', 1, 'Option', 2),
    ('DisableUiDirCheck', 0, 'FeatureToggle', 0),
]
N = C + 7
tbl = bytearray(); strs = bytearray()
str_at = D + 24 * (len(OPTS) + 1)
for key, typ, s, flag in OPTS:
    kva = str_at + len(strs); strs += key.encode() + b'\0'
    tbl += struct.pack('<iIIiII', kva - N, len(key), typ, STR[s] - N, len(s), flag)
tbl += b'\0' * 24
blob_d = bytes(tbl + strs)
assert len(blob_d) <= room_d, (len(blob_d), room_d)

c = bytearray()
c += b'\x53\x56'
c += b'\xE8\x00\x00\x00\x00\x5E'
assert C + len(c) == N + 1
c += b'\x8D\x9E' + struct.pack('<i', D - N)
loop = len(c)
c += b'\x8B\x43\x04\x85\xC0'
jz_at = len(c); c += b'\x74\x00'
c += b'\x83\xEC\x38\x8B\xD4'
c += b'\x8B\x0B\x03\xCE\x89\x0A'
c += b'\x89\x42\x10'
c += b'\x83\xF8\x10\x73\x05\xB8\x10\x00\x00\x00\x89\x42\x14'
c += b'\x8B\x43\x08\x89\x42\x18'
c += b'\x8B\x4B\x0C\x03\xCE\x89\x4A\x1C'
c += b'\x8B\x43\x10\x89\x42\x2C'
c += b'\x83\xF8\x10\x73\x05\xB8\x10\x00\x00\x00\x89\x42\x30'
c += b'\x8A\x43\x14\x88\x42\x34'
c += b'\x8B\xCA\x52\x57'
at = C + len(c); c += b'\xE8' + struct.pack('<i', INSERT - (at + 5))
c += b'\x83\xC4\x38\x83\xC3\x18'
c += b'\xEB' + struct.pack('<b', loop - (len(c) + 2))
done = len(c); c[jz_at + 1] = done - (jz_at + 2)
c += b'\x8D\x86' + struct.pack('<i', ORIG_PUSH - N)
c += b'\x5E\x5B\x50'
at = C + len(c); c += b'\xE9' + struct.pack('<i', BACK - (at + 5))
assert len(c) <= room_c, (len(c), room_c)

data[off(C):off(C)+len(c)] = c
data[off(D):off(D)+len(blob_d)] = blob_d
data[off(HOOK):off(HOOK)+5] = b'\xE9' + struct.pack('<i', C - (HOOK + 5))
struct.pack_into('<I', data, text.get_file_offset() + 8, C + len(c) - (base + text.VirtualAddress))
struct.pack_into('<I', data, rdata.get_file_offset() + 8, D + len(blob_d) - (base + rdata.VirtualAddress))

rd = pe.OPTIONAL_HEADER.DATA_DIRECTORY[5]
o = pe.get_offset_from_rva(rd.VirtualAddress); end = o + rd.Size; killed = 0
while o < end:
    page, size = struct.unpack_from('<II', data, o)
    if size == 0: break
    for i in range((size - 8) // 2):
        eo = o + 8 + 2 * i; e = struct.unpack_from('<H', data, eo)[0]
        if (e >> 12) and HOOK - base <= page + (e & 0xFFF) < HOOK - base + 5:
            assert page + (e & 0xFFF) == HOOK + 1 - base and (e >> 12) == 3
            struct.pack_into('<H', data, eo, 0); killed += 1
    o += size
assert killed == 1
open(OUT, 'wb').write(data)
print('ok: code %d/%d, data %d/%d -> %s' % (len(c), room_c, len(blob_d), room_d, OUT))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for i in md.disasm(bytes(c), C): print('  %08X %s %s' % (i.address, i.mnemonic, i.op_str))
