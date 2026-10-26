using BitMiracle.LibJpeg.Classic;

namespace KFMO;

static class JpegJoint
{
    public sealed class Coefs
    {
        public int Width, Height, Comps;
        public int[] CompId = Array.Empty<int>(), QuantNo = Array.Empty<int>();
        public JBLOCK[][][] Blocks = Array.Empty<JBLOCK[][]>();
        public int BlocksW, BlocksH;
        public byte[] Dqt = Array.Empty<byte>();
    }

    public static Coefs? Read(byte[] jpeg)
    {
        var segs = Segments(jpeg);
        if (segs == null) return null;
        var dqt = new MemoryStream();
        foreach (var (marker, start, len) in segs)
        {
            if (marker == 0xDD || (marker >= 0xC1 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC))
                return null;
            if (marker == 0xDB) dqt.Write(jpeg, start, len);
        }
        var d = new jpeg_decompress_struct(new jpeg_error_mgr());
        d.jpeg_stdio_src(new MemoryStream(jpeg));
        d.jpeg_read_header(true);
        var arr = d.jpeg_read_coefficients();
        var ci = d.Comp_info;
        int nc = d.Num_components;
        var c = new Coefs { Width = d.Image_width, Height = d.Image_height, Comps = nc, CompId = new int[nc], QuantNo = new int[nc], Blocks = new JBLOCK[nc][][], Dqt = dqt.ToArray() };
        c.BlocksW = (c.Width + 7) / 8;
        c.BlocksH = (c.Height + 7) / 8;
        for (int i = 0; i < nc; i++)
        {
            if (ci[i].H_samp_factor != 1 || ci[i].V_samp_factor != 1 || ci[i].Width_in_blocks != c.BlocksW)
                return null;
            c.CompId[i] = ci[i].Component_id;
            c.QuantNo[i] = ci[i].Quant_tbl_no;
            c.Blocks[i] = arr[i].Access(0, c.BlocksH);
        }
        d.jpeg_finish_decompress();
        return c;
    }

    static readonly int[] Natural =
    {
        0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21,
        28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61,
        54, 47, 55, 62, 63,
    };

    static int Category(int v)
    {
        if (v < 0) v = -v;
        int n = 0;
        while (v != 0) { n++; v >>= 1; }
        return n;
    }

    static void Walk(Coefs c, Action<int, bool, int, int> emit)
    {
        var pred = new int[c.Comps];
        for (int by = 0; by < c.BlocksH; by++)
            for (int bx = 0; bx < c.BlocksW; bx++)
                for (int k = 0; k < c.Comps; k++)
                {
                    var blk = c.Blocks[k][by][bx];
                    int dc = blk[0], diff = dc - pred[k];
                    pred[k] = dc;
                    int s = Category(diff);
                    emit(s, false, diff < 0 ? diff - 1 : diff, s);
                    int run = 0;
                    for (int z = 1; z < 64; z++)
                    {
                        int v = blk[Natural[z]];
                        if (v == 0) { run++; continue; }
                        while (run > 15) { emit(0xF0, true, 0, 0); run -= 16; }
                        int sz = Category(v);
                        emit((run << 4) | sz, true, v < 0 ? v - 1 : v, sz);
                        run = 0;
                    }
                    if (run > 0) emit(0x00, true, 0, 0);
                }
    }

    static (byte[] Bits, byte[] Vals) Optimal(long[] freqIn)
    {
        var freq = new long[257];
        Array.Copy(freqIn, freq, 256);
        freq[256] = 1;
        var codesize = new int[257];
        var others = new int[257];
        Array.Fill(others, -1);
        while (true)
        {
            int c1 = -1, c2 = -1;
            long v = long.MaxValue;
            for (int i = 0; i <= 256; i++) if (freq[i] != 0 && freq[i] <= v) { v = freq[i]; c1 = i; }
            v = long.MaxValue;
            for (int i = 0; i <= 256; i++) if (freq[i] != 0 && freq[i] <= v && i != c1) { v = freq[i]; c2 = i; }
            if (c2 < 0) break;
            freq[c1] += freq[c2];
            freq[c2] = 0;
            codesize[c1]++;
            while (others[c1] >= 0) { c1 = others[c1]; codesize[c1]++; }
            others[c1] = c2;
            codesize[c2]++;
            while (others[c2] >= 0) { c2 = others[c2]; codesize[c2]++; }
        }
        var bits = new int[33];
        for (int i = 0; i <= 256; i++) if (codesize[i] != 0) bits[codesize[i]]++;
        for (int i = 32; i > 16; i--)
            while (bits[i] > 0)
            {
                int j = i - 2;
                while (bits[j] == 0) j--;
                bits[i] -= 2; bits[i - 1]++; bits[j + 1] += 2; bits[j]--;
            }
        int last = 16;
        while (bits[last] == 0) last--;
        bits[last]--;
        var vals = new List<byte>();
        for (int len = 1; len <= 32; len++)
            for (int j = 0; j < 256; j++)
                if (codesize[j] == len) vals.Add((byte)j);
        var b = new byte[17];
        for (int i = 1; i <= 16; i++) b[i] = (byte)bits[i];
        return (b, vals.ToArray());
    }

    static (int[] Code, int[] Len) Codes(byte[] bits, byte[] vals)
    {
        var code = new int[256]; var len = new int[256];
        int c = 0, k = 0;
        for (int l = 1; l <= 16; l++)
        {
            for (int i = 0; i < bits[l]; i++) { code[vals[k]] = c; len[vals[k]] = l; k++; c++; }
            c <<= 1;
        }
        return (code, len);
    }

    sealed class BitWriter
    {
        readonly MemoryStream ms;
        int acc, n;
        public BitWriter(MemoryStream s) { ms = s; }
        public void Put(int value, int count)
        {
            for (int i = count - 1; i >= 0; i--)
            {
                acc = (acc << 1) | ((value >> i) & 1);
                if (++n == 8) Flush8();
            }
        }
        void Flush8() { ms.WriteByte((byte)acc); if (acc == 0xFF) ms.WriteByte(0); acc = 0; n = 0; }
        public void Finish() { while (n != 0) { acc = (acc << 1) | 1; if (++n == 8) Flush8(); } }
    }

    public static List<byte[]> Encode(IReadOnlyList<Coefs> mips)
    {
        var dcF = new long[256]; var acF = new long[256];
        foreach (var m in mips)
            Walk(m, (sym, ac, _, _) => { if (ac) acF[sym]++; else dcF[sym]++; });
        var (dcBits, dcVals) = Optimal(dcF);
        var (acBits, acVals) = Optimal(acF);
        var (dcCode, dcLen) = Codes(dcBits, dcVals);
        var (acCode, acLen) = Codes(acBits, acVals);

        var dht = new MemoryStream();
        foreach (var (cls, bits, vals) in new[] { (0x00, dcBits, dcVals), (0x10, acBits, acVals) })
        {
            int len = 2 + 1 + 16 + vals.Length;
            dht.Write(new byte[] { 0xFF, 0xC4, (byte)(len >> 8), (byte)len, (byte)cls });
            dht.Write(bits, 1, 16);
            dht.Write(vals);
        }
        var result = new List<byte[]>();
        foreach (var m in mips)
        {
            var ms = new MemoryStream();
            ms.Write(new byte[] { 0xFF, 0xD8 });
            ms.Write(m.Dqt);
            dht.Position = 0; dht.CopyTo(ms);
            int sofLen = 8 + 3 * m.Comps;
            ms.Write(new byte[] { 0xFF, 0xC0, (byte)(sofLen >> 8), (byte)sofLen, 8, (byte)(m.Height >> 8), (byte)m.Height, (byte)(m.Width >> 8), (byte)m.Width, (byte)m.Comps });
            for (int i = 0; i < m.Comps; i++) ms.Write(new byte[] { (byte)m.CompId[i], 0x11, (byte)m.QuantNo[i] });
            int sosLen = 6 + 2 * m.Comps;
            ms.Write(new byte[] { 0xFF, 0xDA, (byte)(sosLen >> 8), (byte)sosLen, (byte)m.Comps });
            for (int i = 0; i < m.Comps; i++) ms.Write(new byte[] { (byte)m.CompId[i], 0x00 });
            ms.Write(new byte[] { 0, 63, 0 });
            var bw = new BitWriter(ms);
            Walk(m, (sym, ac, extra, extraLen) =>
            {
                if (ac) bw.Put(acCode[sym], acLen[sym]); else bw.Put(dcCode[sym], dcLen[sym]);
                if (extraLen > 0) bw.Put(extra & ((1 << extraLen) - 1), extraLen);
            });
            bw.Finish();
            ms.Write(new byte[] { 0xFF, 0xD9 });
            result.Add(ms.ToArray());
        }
        return result;
    }

    static List<(int Marker, int Start, int Len)>? Segments(byte[] j)
    {
        var list = new List<(int, int, int)>();
        if (j.Length < 4 || j[0] != 0xFF || j[1] != 0xD8) return null;
        int i = 2;
        while (i + 4 <= j.Length)
        {
            if (j[i] != 0xFF) return null;
            int m = j[i + 1];
            if (m == 0xFF) { i++; continue; }
            int len = (j[i + 2] << 8) | j[i + 3];
            list.Add((m, i, len + 2));
            if (m == 0xDA) return list;
            i += 2 + len;
        }
        return null;
    }
}
