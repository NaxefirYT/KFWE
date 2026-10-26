using System.Text;
using LibDeflate;

namespace KFMO;

static class MpqWriter
{
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
        {
            string name = all[i].Name;
            uint start = Hash(name, 0) & (uint)(hashSize - 1);
            for (uint k = 0; ; k++)
            {
                uint slot = (start + k) & (uint)(hashSize - 1);
                if (hashTable[slot * 4 + 3] == 0xFFFFFFFF)
                {
                    hashTable[slot * 4 + 0] = Hash(name, 1);
                    hashTable[slot * 4 + 1] = Hash(name, 2);
                    hashTable[slot * 4 + 2] = 0;
                    hashTable[slot * 4 + 3] = (uint)blockOf[i];
                    break;
                }
            }
        }
        Encrypt(hashTable, Hash("(hash table)", 3));
        Encrypt(blockTable, Hash("(block table)", 3));
        uint hashPos = (uint)(ms.Position - mpqStart);
        foreach (var v in hashTable) ms.Write(BitConverter.GetBytes(v));
        uint blockPos = (uint)(ms.Position - mpqStart);
        foreach (var v in blockTable) ms.Write(BitConverter.GetBytes(v));
        uint archiveSize = (uint)(ms.Position - mpqStart);

        ms.Position = mpqStart;
        using (var w = new BinaryWriter(ms, Encoding.ASCII, true))
        {
            w.Write(Encoding.ASCII.GetBytes("MPQ\x1A"));
            w.Write(32u);
            w.Write(archiveSize);
            w.Write((ushort)0);
            w.Write((ushort)sectorShift);
            w.Write(hashPos);
            w.Write(blockPos);
            w.Write((uint)hashSize);
            w.Write((uint)uniq.Count);
        }
        return ms.ToArray();
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
