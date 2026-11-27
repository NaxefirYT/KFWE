import os
import sys
import zipfile

SKIP_DIRS = {'YDWE', 'backups', 'logs', 'unknownui'}
SKIP_FILES = {
    'bin/playas.cfg',
    'bin/YDWEConfig.orig.exe',
    'bin/changelog.seen',
    'plugin/w3x2lni/plugin/\u00b5\u00f9\u00d1\u00d5+\u00f9\u00de\u00c0\u00bb\u0565\u00e4.lua',
    'plugin/tesh/.gitignore',
    'plugin/tesh/Changelog - Horus.txt',
    'plugin/tesh/Changelog - TESH.txt',
    'plugin/tesh/Changelog.txt',
    'plugin/tesh/funclist.db',
    'plugin/tesh/horus.dll',
    'plugin/tesh/license.txt',
    'plugin/tesh/pjass.exe',
    'plugin/tesh/SciLexer.dll',
    'plugin/tesh/Styles.ini',
    'plugin/tesh/TESH.conf',
    'plugin/tesh/TESH.ini',
    'plugin/tesh/TESH_en-US.conf',
    'plugin/tesh/TESH_EN.conf',
    'plugin/tesh/TESH_zh-CN.conf',
}
SKIP_FILES_LOWER = {p.lower() for p in SKIP_FILES}
SKIP_EXT = ('.pdb', '.lib', '.orig')


def wanted(rel):
    parts = rel.split('/')
    if parts[0] in SKIP_DIRS or parts[0].startswith('backup-'):
        return False
    if 'src' in parts[:-1] or rel.lower() in SKIP_FILES_LOWER or rel.endswith(SKIP_EXT):
        return False
    return '.bak' not in parts[-1] and '.old-running' not in parts[-1]


def large_address_aware(data):
    pe = int.from_bytes(data[0x3C:0x40], 'little')
    assert data[pe:pe + 4] == b'PE\0\0', 'worldedit.exe не PE'
    flags = int.from_bytes(data[pe + 22:pe + 24], 'little') | 0x20
    return data[:pe + 22] + flags.to_bytes(2, 'little') + data[pe + 24:]


def main():
    src, out = sys.argv[1], sys.argv[2]
    n = 0
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for root, dirs, files in os.walk(src):
            dirs.sort()
            for f in sorted(files):
                full = os.path.join(root, f)
                rel = os.path.relpath(full, src).replace(os.sep, '/')
                if not wanted(rel):
                    continue
                if rel == 'bin/worldedit.exe':
                    with open(full, 'rb') as fh:
                        z.writestr(zipfile.ZipInfo.from_file(full, rel), large_address_aware(fh.read()), zipfile.ZIP_DEFLATED, 9)
                else:
                    z.write(full, rel)
                n += 1
    assert n and 'YDWE.exe' in zipfile.ZipFile(out).namelist(), 'в папке нет YDWE.exe'
    print(n, 'файлов,', os.path.getsize(out) // 1048576, 'МБ')


if __name__ == '__main__':
    main()
