import os
import sys
import zipfile

SKIP_DIRS = {'YDWE', 'backups', 'logs', 'unknownui'}
SKIP_FILES = {'bin/playas.cfg', 'bin/YDWEConfig.orig.exe', 'bin/changelog.seen'}
SKIP_EXT = ('.pdb', '.lib', '.orig')


def wanted(rel):
    parts = rel.split('/')
    if parts[0] in SKIP_DIRS or parts[0].startswith('backup-'):
        return False
    if 'src' in parts[:-1] or rel in SKIP_FILES or rel.endswith(SKIP_EXT):
        return False
    return '.bak' not in parts[-1] and '.old-running' not in parts[-1]


def main():
    src, out = sys.argv[1], sys.argv[2]
    n = 0
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for root, dirs, files in os.walk(src):
            dirs.sort()
            for f in sorted(files):
                full = os.path.join(root, f)
                rel = os.path.relpath(full, src).replace(os.sep, '/')
                if wanted(rel):
                    z.write(full, rel)
                    n += 1
    assert n and 'YDWE.exe' in zipfile.ZipFile(out).namelist(), 'в папке нет YDWE.exe'
    print(n, 'файлов,', os.path.getsize(out) // 1048576, 'МБ')


if __name__ == '__main__':
    main()
