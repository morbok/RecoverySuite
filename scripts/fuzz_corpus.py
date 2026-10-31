"""Generate small valid disk fixtures; no external data or device access."""
from pathlib import Path
import sys


def put(data, offset, value, width=2):
    data[offset:offset + width] = value.to_bytes(width, 'little')


def fixtures():
    fat = bytearray(64 * 512)
    put(fat, 11, 512); fat[13] = 1; put(fat, 14, 1); fat[16] = 2
    put(fat, 17, 16); put(fat, 19, 64); fat[21] = 0xF8; put(fat, 22, 1)
    put(fat, 510, 0xAA55)
    for offset in (512, 1024):
        fat[offset:offset + 6] = bytes.fromhex('f8ffffff0f00')
    pos = 3 * 512
    fat[pos:pos + 11] = b'HELLO   TXT'; fat[pos + 11] = 0x20
    put(fat, pos + 26, 2); put(fat, pos + 28, 5, 4)
    fat[4 * 512:4 * 512 + 5] = b'hello'
    ntfs = bytearray(128 * 512)
    ntfs[3:11] = b'NTFS    '; put(ntfs, 11, 512); ntfs[13] = 1
    put(ntfs, 40, 128, 8); put(ntfs, 48, 8, 8); ntfs[64] = 0xF6
    put(ntfs, 510, 0xAA55)

    def blank(flags=1, sequence=1):
        record = bytearray(1024); record[:4] = b'FILE'
        put(record, 4, 48); put(record, 6, 3); put(record, 16, sequence)
        put(record, 20, 56); put(record, 22, flags); put(record, 28, 1024, 4)
        put(record, 24, 64, 4); put(record, 56, 0xFFFFFFFF, 4)
        return record

    def protect(record):
        put(record, 48, 0xBEEF)
        for index in (1, 2):
            record[48 + 2 * index:50 + 2 * index] = record[512 * index - 2:512 * index]
            put(record, 512 * index - 2, 0xBEEF)
        return record

    records = [bytearray(1024) for _ in range(18)]
    base = blank(); p = 56
    put(base, p, 0x80, 4); put(base, p + 4, 80, 4); base[p + 8] = 1
    put(base, p + 24, 35, 8); put(base, p + 32, 64)
    for offset in (40, 48, 56):
        put(base, p + offset, 18 * 1024, 8)
    base[p + 64:p + 68] = bytes([0x11, 36, 8, 0])
    put(base, p + 80, 0xFFFFFFFF, 4); put(base, 24, p + 88, 4)
    records[0] = base; records[5] = blank(3, 7)
    file = blank(); p = 56

    def resident(kind, value):
        nonlocal p
        length = (24 + len(value) + 7) & ~7
        put(file, p, kind, 4); put(file, p + 4, length, 4)
        put(file, p + 16, len(value), 4); put(file, p + 20, 24)
        file[p + 24:p + 24 + len(value)] = value
        p += length

    name = 'HELLO.TXT'.encode('utf-16le'); fn = bytearray(66 + len(name))
    put(fn, 0, 5 | (7 << 48), 8); put(fn, 48, 5, 8); put(fn, 56, 0x20, 4)
    fn[64] = len(name) // 2; fn[65] = 1; fn[66:] = name
    resident(0x10, bytes(72)); resident(0x30, fn); resident(0x80, b'hello')
    put(file, p, 0xFFFFFFFF, 4); put(file, 24, p + 8, 4); records[16] = file
    for index, record in enumerate(records):
        if record[:4] == b'FILE':
            protect(record)
        offset = 8 * 512 + index * 1024
        ntfs[offset:offset + 1024] = record
    signature = bytearray(512); signature[:10] = bytes.fromhex('ffd8ff68656c6c6fffd9')
    return {'fat12.img': bytes(fat), 'ntfs.img': bytes(ntfs), 'signature.img': bytes(signature)}


if __name__ == '__main__':
    target = Path(sys.argv[1]); target.mkdir(parents=True, exist_ok=True)
    for name, data in fixtures().items():
        (target / name).write_bytes(data)
