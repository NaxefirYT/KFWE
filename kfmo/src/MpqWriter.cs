using System.Runtime.InteropServices;
using System.Text;
using LibDeflate;

namespace KFMO;

static class MpqWriter
{
    public readonly record struct FileEntry(string Name, string DiskPath, long Size, bool Raw);

    const int LargeFileThreshold = 4 * 1024 * 1024;
    const long BatchByteLimit = 32 * 1024 * 1024;
    const int BatchFileLimit = 256;
    static readonly uint[] Crypt = BuildCryptTable();

    static uint[] BuildCryptTable()
    {
        var t = new uint[0x500];
        uint seed = 0x00100001;
        for (int i = 0; i < 0x100; i++)
            for (int j = 0, idx = i; j < 5; j++, idx += 0x100)
            {
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint a = (seed & 0xFFFF) << 16;
                seed = (seed * 125 + 3) % 0x2AAAAB;
                t[idx] = a | (seed & 0xFFFF);
            }
        return t;
    }

    public static uint Hash(string s, int type)
    {
        uint s1 = 0x7FED7FED, s2 = 0xEEEEEEEE;
        foreach (char ch0 in s)
        {
            uint c = (uint)(ch0 & 0xFF);
            if (c >= 'a' && c <= 'z') c -= 32;
            else if (c == '/') c = '\\';
            s1 = Crypt[(type << 8) + c] ^ (s1 + s2);
            s2 = c + s1 + s2 + (s2 << 5) + 3;
        }
        return s1;
    }

    static void Encrypt(uint[] data, uint key)
    {
        uint s2 = 0xEEEEEEEE;
        for (int i = 0; i < data.Length; i++)
        {
            s2 += Crypt[0x400 + (key & 0xFF)];
            uint plain = data[i];
            data[i] = plain ^ (key + s2);
            key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
            s2 = plain + s2 + (s2 << 5) + 3;
        }
    }

    public static int Deduplicated;

    public static byte[] Write(IReadOnlyList<(string Name, byte[] Data)> files, byte[]? hm3wHeader, int level, bool listfile, int sectorShift = 7, bool dedup = true)
    {
        int sectorSize = 512 << sectorShift;
        var all = files.ToList();
        if (listfile)
        {
            all.RemoveAll(f => f.Name.Equals("(listfile)", StringComparison.OrdinalIgnoreCase));
            var lf = string.Join("\r\n", all.Select(f => f.Name)) + "\r\n";
            all.Add(("(listfile)", Encoding.Latin1.GetBytes(lf)));
        }

        var blockOf = new int[all.Count];
        var uniq = new List<int>();
        var seen = new Dictionary<string, int>();
        for (int i = 0; i < all.Count; i++)
        {
            var d = all[i].Data;
            string key = !dedup || d.Length == 0 ? "" : d.Length + ":" + Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(d));
            if (dedup && d.Length > 0 && seen.TryGetValue(key, out int b)) { blockOf[i] = b; continue; }
            blockOf[i] = uniq.Count;
            if (d.Length > 0) seen[key] = uniq.Count;
            uniq.Add(i);
        }
        Deduplicated = all.Count - uniq.Count;

        var blobs = new byte[uniq.Count][];
        Parallel.For(0, uniq.Count, () => new ZlibCompressor(level), (u, _, comp) =>
        {
            blobs[u] = PackFile(all[uniq[u]].Data, sectorSize, comp);
            return comp;
        }, comp => comp.Dispose());

        int hashSize = 16;
        while (hashSize < all.Count * 2) hashSize <<= 1;
        var hashTable = new uint[hashSize * 4];
        Array.Fill(hashTable, 0xFFFFFFFF);
        var blockTable = new uint[uniq.Count * 4];

        using var ms = new MemoryStream();
        if (hm3wHeader != null) ms.Write(hm3wHeader);
        long mpqStart = ms.Position;
        ms.Write(new byte[32]);
        for (int u = 0; u < uniq.Count; u++)
        {
            uint offset = (uint)(ms.Position - mpqStart);
            ms.Write(blobs[u]);
            uint size = (uint)all[uniq[u]].Data.Length;
            blockTable[u * 4 + 0] = offset;
            blockTable[u * 4 + 1] = (uint)blobs[u].Length;
            blockTable[u * 4 + 2] = size;
            blockTable[u * 4 + 3] = size == 0 ? 0x80000000u : 0x80000200u;
        }
        for (int i = 0; i < all.Count; i++)
            InsertHash(hashTable, hashSize, all[i].Name, (uint)blockOf[i]);
        Encrypt(hashTable, Hash("(hash table)", 3));
        Encrypt(blockTable, Hash("(block table)", 3));
        uint hashPos = (uint)(ms.Position - mpqStart);
        foreach (var v in hashTable) ms.Write(BitConverter.GetBytes(v));
        uint blockPos = (uint)(ms.Position - mpqStart);
        foreach (var v in blockTable) ms.Write(BitConverter.GetBytes(v));
        uint archiveSize = (uint)(ms.Position - mpqStart);

        ms.Position = mpqStart;
        using (var w = new BinaryWriter(ms, Encoding.ASCII, true))
            WriteMpqHeader(w, archiveSize, (ushort)sectorShift, hashPos, blockPos, (uint)hashSize, (uint)uniq.Count);
        return ms.ToArray();
    }

    static uint CheckUInt32(long v) =>
        v is >= 0 and <= uint.MaxValue ? (uint)v : throw new InvalidDataException("MPQ v1 size limit (4 GB) exceeded");

    static void InsertHash(uint[] hashTable, int hashSize, string name, uint blockIndex)
    {
        uint start = Hash(name, 0) & (uint)(hashSize - 1);
        for (uint k = 0; ; k++)
        {
            uint slot = (start + k) & (uint)(hashSize - 1);
            if (hashTable[slot * 4 + 3] == 0xFFFFFFFF)
            {
                hashTable[slot * 4 + 0] = Hash(name, 1);
                hashTable[slot * 4 + 1] = Hash(name, 2);
                hashTable[slot * 4 + 2] = 0;
                hashTable[slot * 4 + 3] = blockIndex;
                break;
            }
        }
    }

    static void WriteMpqHeader(BinaryWriter w, uint archiveSize, ushort sectorShift, uint hashPos, uint blockPos, uint hashSize, uint blockCount)
    {
        w.Write(Encoding.ASCII.GetBytes("MPQ\x1A"));
        w.Write(32u);
        w.Write(archiveSize);
        w.Write((ushort)0);
        w.Write(sectorShift);
        w.Write(hashPos);
        w.Write(blockPos);
        w.Write(hashSize);
        w.Write(blockCount);
    }

    /// <summary>Writes an MPQ v1 archive directly to a seekable stream with bounded memory usage.</summary>
    public static void WriteStream(IReadOnlyList<FileEntry> entries, Stream output, byte[]? hm3wHeader, int level, bool listfile, int sectorShift = 7)
    {
        if (!output.CanSeek || !output.CanWrite)
            throw new ArgumentException("Stream must be seekable and writable", nameof(output));

        int sectorSize = 512 << sectorShift;
        var list = listfile
            ? entries.Where(e => !e.Name.Equals("(listfile)", StringComparison.OrdinalIgnoreCase)).ToList()
            : entries.ToList();

        if (hm3wHeader != null) output.Write(hm3wHeader);
        long mpqStart = output.Position;
        Span<byte> zeroHdr = stackalloc byte[32];
        zeroHdr.Clear();
        output.Write(zeroHdr);

        int totalCount = list.Count + (listfile ? 1 : 0);
        int hashSize = 16;
        while (hashSize < totalCount * 2) hashSize <<= 1;
        var hashTable = new uint[hashSize * 4];
        Array.Fill(hashTable, 0xFFFFFFFFu);
        var blockTable = new uint[totalCount * 4];

        var copyBuf = new byte[65536];
        ZlibCompressor? streamComp = null;
        byte[]? rawSecBuf = null;
        byte[]? cmpSecBuf = null;
        var batch = new List<(int Index, FileEntry Entry)>();
        long batchBytes = 0;

        void SetBlock(int idx, uint offset, uint packedSize, uint origSize, uint flags)
        {
            blockTable[idx * 4 + 0] = offset;
            blockTable[idx * 4 + 1] = packedSize;
            blockTable[idx * 4 + 2] = origSize;
            blockTable[idx * 4 + 3] = flags;
        }

        void FlushBatch()
        {
            if (batch.Count == 0) return;
            var blobs = new byte[batch.Count][];
            var sizes = new int[batch.Count];
            Parallel.For(0, batch.Count, () => new ZlibCompressor(level), (b, _, comp) =>
            {
                var raw = File.ReadAllBytes(batch[b].Entry.DiskPath);
                sizes[b] = raw.Length;
                blobs[b] = PackFile(raw, sectorSize, comp);
                return comp;
            }, comp => comp.Dispose());

            for (int b = 0; b < batch.Count; b++)
            {
                int idx = batch[b].Index;
                uint offset = CheckUInt32(output.Position - mpqStart);
                var blob = blobs[b];
                output.Write(blob);
                uint orig = (uint)sizes[b];
                SetBlock(idx, offset, CheckUInt32(blob.Length), orig, orig == 0 ? 0x80000000u : 0x80000200u);
                blobs[b] = null!;
            }
            batch.Clear();
            batchBytes = 0;
        }

        try
        {
            for (int i = 0; i < list.Count; i++)
            {
                var e = list[i];
                if (e.Raw || e.Size == 0)
                {
                    FlushBatch();
                    uint offset = CheckUInt32(output.Position - mpqStart);
                    long actualSize = 0;
                    using (var fs = File.OpenRead(e.DiskPath))
                    {
                        int n;
                        while ((n = fs.Read(copyBuf, 0, copyBuf.Length)) > 0)
                        {
                            output.Write(copyBuf, 0, n);
                            actualSize += n;
                        }
                    }
                    uint sz = CheckUInt32(actualSize);
                    SetBlock(i, offset, sz, sz, 0x80000000u);
                }
                else if (e.Size > LargeFileThreshold)
                {
                    FlushBatch();
                    streamComp ??= new ZlibCompressor(level);
                    rawSecBuf ??= new byte[sectorSize];
                    cmpSecBuf ??= new byte[streamComp.GetBound(sectorSize) + 1];

                    using var fs = File.OpenRead(e.DiskPath);
                    long fileLen = fs.Length;
                    uint orig = CheckUInt32(fileLen);
                    long startPos = output.Position;
                    uint offset = CheckUInt32(startPos - mpqStart);
                    if (fileLen == 0)
                    {
                        SetBlock(i, offset, 0, 0, 0x80000000u);
                        continue;
                    }

                    int sectors = (int)((fileLen + sectorSize - 1) / sectorSize);
                    var offsets = new uint[sectors + 1];
                    output.Write(MemoryMarshal.AsBytes(offsets.AsSpan()));

                    for (int s = 0; s < sectors; s++)
                    {
                        offsets[s] = CheckUInt32(output.Position - startPos);
                        int toRead = (int)Math.Min(sectorSize, fileLen - (long)s * sectorSize);
                        int got = fs.ReadAtLeast(rawSecBuf.AsSpan(0, toRead), toRead, throwOnEndOfStream: true);
                        int clen = streamComp.Compress(rawSecBuf.AsSpan(0, got), cmpSecBuf.AsSpan(1));
                        if (clen > 0 && clen + 1 < got)
                        {
                            cmpSecBuf[0] = 0x02;
                            output.Write(cmpSecBuf, 0, clen + 1);
                        }
                        else
                            output.Write(rawSecBuf, 0, got);
                    }

                    long endPos = output.Position;
                    offsets[sectors] = CheckUInt32(endPos - startPos);
                    output.Position = startPos;
                    output.Write(MemoryMarshal.AsBytes(offsets.AsSpan()));
                    output.Position = endPos;
                    SetBlock(i, offset, CheckUInt32(endPos - startPos), orig, 0x80000200u);
                }
                else
                {
                    batch.Add((i, e));
                    batchBytes += e.Size;
                    if (batchBytes >= BatchByteLimit || batch.Count >= BatchFileLimit)
                        FlushBatch();
                }
            }

            FlushBatch();

            if (listfile)
            {
                streamComp ??= new ZlibCompressor(level);
                var lf = string.Join("\r\n", list.Select(e => e.Name)) + "\r\n";
                var lfBytes = Encoding.Latin1.GetBytes(lf);
                var lfBlob = PackFile(lfBytes, sectorSize, streamComp);
                uint offset = CheckUInt32(output.Position - mpqStart);
                output.Write(lfBlob);
                uint orig = CheckUInt32(lfBytes.Length);
                SetBlock(list.Count, offset, CheckUInt32(lfBlob.Length), orig, orig == 0 ? 0x80000000u : 0x80000200u);
            }
        }
        finally
        {
            streamComp?.Dispose();
        }

        for (int i = 0; i < list.Count; i++)
            InsertHash(hashTable, hashSize, list[i].Name, (uint)i);
        if (listfile)
            InsertHash(hashTable, hashSize, "(listfile)", (uint)list.Count);

        Encrypt(hashTable, Hash("(hash table)", 3));
        Encrypt(blockTable, Hash("(block table)", 3));

        uint hashPos = CheckUInt32(output.Position - mpqStart);
        output.Write(MemoryMarshal.AsBytes(hashTable.AsSpan()));
        uint blockPos = CheckUInt32(output.Position - mpqStart);
        output.Write(MemoryMarshal.AsBytes(blockTable.AsSpan()));
        uint archiveSize = CheckUInt32(output.Position - mpqStart);

        output.Position = mpqStart;
        using (var w = new BinaryWriter(output, Encoding.ASCII, true))
            WriteMpqHeader(w, archiveSize, (ushort)sectorShift, hashPos, blockPos, (uint)hashSize, (uint)totalCount);
        output.Flush();
    }

    internal static byte[] PackFile(byte[] data, int sectorSize, Compressor comp)
    {
        if (data.Length == 0) return Array.Empty<byte>();
        int n = (data.Length + sectorSize - 1) / sectorSize;
        var offsets = new uint[n + 1];
        var body = new MemoryStream();
        int tableBytes = (n + 1) * 4;
        var buf = new byte[comp.GetBound(sectorSize) + 1];
        for (int s = 0; s < n; s++)
        {
            offsets[s] = (uint)(tableBytes + body.Length);
            var raw = data.AsSpan(s * sectorSize, Math.Min(sectorSize, data.Length - s * sectorSize));
            int len = comp.Compress(raw, buf.AsSpan(1));
            if (len > 0 && len + 1 < raw.Length)
            {
                buf[0] = 0x02;
                body.Write(buf, 0, len + 1);
            }
            else
                body.Write(raw);
        }
        offsets[n] = (uint)(tableBytes + body.Length);
        var res = new byte[tableBytes + body.Length];
        Buffer.BlockCopy(offsets, 0, res, 0, tableBytes);
        body.Position = 0;
        body.Read(res, tableBytes, (int)body.Length);
        return res;
    }
}
