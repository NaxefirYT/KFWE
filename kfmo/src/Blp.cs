using System.Text;

namespace KFMO;

sealed class Blp1
{
    public int Compression;
    public int AlphaBits;
    public int Width, Height;
    public int Extra, HasMipmaps;
    public byte[] JpegHeader = Array.Empty<byte>();
    public List<byte[]> Mips = new();

    public bool IsJpeg => Compression == 0;

    public static Blp1? Parse(byte[] d)
    {
        if (d.Length < 160 || Encoding.ASCII.GetString(d, 0, 4) != "BLP1")
            return null;
        var b = new Blp1
        {
            Compression = BitConverter.ToInt32(d, 4),
            AlphaBits = BitConverter.ToInt32(d, 8),
            Width = BitConverter.ToInt32(d, 12),
            Height = BitConverter.ToInt32(d, 16),
            Extra = BitConverter.ToInt32(d, 20),
            HasMipmaps = BitConverter.ToInt32(d, 24),
        };
        if (!b.IsJpeg)
            return b;
        int hdrSize = BitConverter.ToInt32(d, 156);
        if (hdrSize < 0 || 160 + hdrSize > d.Length)
            return null;
        b.JpegHeader = d.AsSpan(160, hdrSize).ToArray();
        for (int i = 0; i < 16; i++)
        {
            int off = BitConverter.ToInt32(d, 28 + i * 4), size = BitConverter.ToInt32(d, 92 + i * 4);
            if (size <= 0 || off <= 0)
                break;
            if (off + size > d.Length)
                return null;
            b.Mips.Add(d.AsSpan(off, size).ToArray());
        }
        return b.Mips.Count > 0 ? b : null;
    }

    public byte[] MipJpeg(int i) => [.. JpegHeader, .. Mips[i]];

    public byte[] BuildFromJpegs(IReadOnlyList<byte[]> jpegs)
    {
        int prefix = jpegs[0].Length;
        foreach (var j in jpegs)
        {
            int n = Math.Min(prefix, j.Length), k = 0;
            while (k < n && j[k] == jpegs[0][k]) k++;
            prefix = k;
        }
        prefix = Math.Min(prefix, 624);
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(Encoding.ASCII.GetBytes("BLP1"));
        w.Write(0); w.Write(AlphaBits); w.Write(Width); w.Write(Height); w.Write(Extra); w.Write(HasMipmaps);
        var offsets = new int[jpegs.Count];
        var unique = new List<int>();
        int pos = 160 + prefix;
        for (int i = 0; i < jpegs.Count; i++)
        {
            int same = unique.FindIndex(u => jpegs[u].AsSpan(prefix).SequenceEqual(jpegs[i].AsSpan(prefix)));
            if (same >= 0) { offsets[i] = offsets[unique[same]]; continue; }
            offsets[i] = pos;
            pos += jpegs[i].Length - prefix;
            unique.Add(i);
        }
        for (int i = 0; i < 16; i++) w.Write(i < jpegs.Count ? offsets[i] : 0);
        for (int i = 0; i < 16; i++) w.Write(i < jpegs.Count ? jpegs[i].Length - prefix : 0);
        w.Write(prefix);
        w.Write(jpegs[0], 0, prefix);
        foreach (int u in unique)
            w.Write(jpegs[u], prefix, jpegs[u].Length - prefix);
        return ms.ToArray();
    }
}
