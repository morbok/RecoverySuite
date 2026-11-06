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


def exfat_fixture():
    image = bytearray(128 * 512)
    image[3:11] = b'EXFAT   '
    for offset, value, width in ((72,128,8),(80,24,4),(84,1,4),
                                  (88,25,4),(92,103,4),(96,2,4),(104,256,2),(510,0xAA55,2)):
        put(image,offset,value,width)
    image[108] = 9; image[110] = 1
    for cluster in (2,3,4): put(image,24*512+cluster*4,0xFFFFFFFF,4)
    root=25*512; image[root]=0x81
    put(image,root+20,3,4); put(image,root+24,13,8); image[26*512]=7
    entry=bytearray(96); entry[0]=0x85; entry[1]=2; put(entry,4,0x20)
    entry[32]=0xC0; entry[33]=3; entry[35]=9
    put(entry,40,5,8); put(entry,52,4,4); put(entry,56,5,8)
    entry[64]=0xC1; entry[66:84]='notes.txt'.encode('utf-16le')
    checksum=0
    for i,value in enumerate(entry):
        if i not in (2,3): checksum=((checksum>>1)+((checksum&1)<<15)+value)&0xFFFF
    put(entry,2,checksum); image[root+32:root+128]=entry
    image[27*512:27*512+5]=b'hello'
    checksum=0
    for i,value in enumerate(image[:11*512]):
        if i not in (106,107,112): checksum=((checksum>>1)+((checksum&1)<<31)+value)&0xFFFFFFFF
    for i in range(11*512,12*512,4): put(image,i,checksum,4)
    return bytes(image)


def ext4_fixture():
    image=bytearray(128*1024);sb=1024
    for at,value,width in ((0,16,4),(4,128,4),(20,1,4),(32,128,4),(40,16,4),(56,0xEF53,2),(88,128,2),(96,0x42,4)):
        put(image,sb+at,value,width)
    for at,value in ((2048,3),(2052,4),(2056,5)):put(image,at,value,4)
    image[3*1024]=255;put(image,4*1024,0x402)
    for number,mode,size,physical in ((2,0x41ED,1024,7),(11,0x81A4,5,8)):
        at=5*1024+(number-1)*128
        for off,value,width in ((0,mode,2),(4,size,4),(26,1,2),(32,0x80000,4),(40,0xF30A,2),(42,1,2),(44,4,2),(56,1,2),(60,physical,4)):
            put(image,at+off,value,width)
    root=7*1024
    for off,number,length,name in ((0,2,12,b'.'),(12,2,12,b'..'),(24,11,1000,b'notes.txt')):
        put(image,root+off,number,4);put(image,root+off+4,length)
        image[root+off+6]=len(name);image[root+off+7]=2 if number==2 else 1
        image[root+off+8:root+off+8+len(name)]=name
    image[8*1024:8*1024+5]=b'hello'
    return bytes(image)


def gpt_fixture():
    import zlib
    image = bytearray(64 * 512)
    image[450] = 0xEE; put(image, 454, 1, 4); put(image, 458, 63, 4)
    put(image, 510, 0xAA55)
    header = bytearray(512); header[:8] = b'EFI PART'
    put(header, 8, 0x10000, 4); put(header, 12, 92, 4)
    put(header, 24, 1, 8); put(header, 32, 63, 8)
    put(header, 40, 3, 8); put(header, 48, 61, 8)
    header[56:72] = bytes(range(1, 17))
    put(header, 72, 2, 8); put(header, 80, 4, 4); put(header, 84, 128, 4)
    put(header, 88, zlib.crc32(bytes(512)), 4)
    put(header, 16, zlib.crc32(header[:92]), 4)
    image[512:1024] = header
    put(header, 24, 63, 8); put(header, 32, 1, 8); put(header, 16, 0, 4)
    put(header, 16, zlib.crc32(header[:92]), 4)
    image[-512:] = header
    return bytes(image)


if __name__ == '__main__':
    target = Path(sys.argv[1]); target.mkdir(parents=True, exist_ok=True)
    corpus = fixtures()
    fat4k = bytearray(4096)
    fat4k[:512] = corpus['fat12.img'][:512]
    put(fat4k, 11, 4096)
    zero_cluster = bytearray(corpus['fat12.img'])
    zero_cluster[13] = 0
    for name, data in {**corpus, "gpt.img": gpt_fixture(), "exfat.img": exfat_fixture(), "ext4.img": ext4_fixture(),
                       "fat-4k-boot.img": fat4k,
                       "fat-zero-cluster.img": zero_cluster}.items():
        (target / name).write_bytes(data)
