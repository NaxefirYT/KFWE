using System.Text;

namespace KFMO;

static class MdxQuant
{
    public sealed class Bits
    {
        public int Normals = 8, Rotation = 11, Translation = 10, Scaling = 10;
        public int Vertices = 23, Uv = 23, Extents = 23;
        public bool Truncate;

        public static Bits Release() => new() { Normals = 7, Rotation = 7, Translation = 7, Scaling = 7, Vertices = 7, Uv = 7, Extents = 7 };
    }

    public sealed class Stats { public long Normals, Keys; }

    public static bool Apply(byte[] d, Bits bits, Stats st)
    {
        if (d.Length < 8 || Encoding.ASCII.GetString(d, 0, 4) != "MDLX") return false;
        var copy = (byte[])d.Clone();
        truncate = bits.Truncate;
        long n0 = st.Normals, k0 = st.Keys;
        try
        {
            for (int i = 4; i + 8 <= d.Length;)
            {
                string tag = Encoding.ASCII.GetString(d, i, 4);
                int size = BitConverter.ToInt32(d, i + 4), s = i + 8, e = s + size;
                if (size < 0 || e > d.Length) throw new InvalidDataException();
                switch (tag)
                {
                    case "GEOS": Geosets(d, s, e, bits, st); break;
                    case "BONE": for (int p = s; p < e;) p = Node(d, p, e, bits, st) + 8; break;
                    case "HELP": for (int p = s; p < e;) p = Node(d, p, e, bits, st); break;
                }
                i = e;
            }
            return true;
        }
        catch
        {
            Buffer.BlockCopy(copy, 0, d, 0, d.Length);
            st.Normals = n0; st.Keys = k0;
            return false;
        }
    }

    [ThreadStatic] static bool truncate;

    static void Q(byte[] d, int p, int keep)
    {
        if (keep >= 23) return;
        if (truncate)
        {
            uint t = BitConverter.ToUInt32(d, p) & ~((1u << (23 - keep)) - 1);
            BitConverter.TryWriteBytes(d.AsSpan(p, 4), t);
            return;
        }
        uint v = BitConverter.ToUInt32(d, p);
        if ((v & 0x7F800000) == 0x7F800000) return;
        int drop = 23 - keep;
        uint half = 1u << (drop - 1), mask = ~((1u << drop) - 1);
        uint r = (v + half) & mask;
        if ((r & 0x7F800000) == 0x7F800000) return;
        BitConverter.TryWriteBytes(d.AsSpan(p, 4), r);
    }

    static void Geosets(byte[] d, int s, int e, Bits bits, Stats st)
    {
        for (int g = s; g < e;)
        {
            int gsize = BitConverter.ToInt32(d, g);
            if (gsize <= 4 || g + gsize > e) throw new InvalidDataException();
            int p = g + 4, ge = g + gsize;
            Expect(d, p, "VRTX"); int nv = BitConverter.ToInt32(d, p + 4);
            if (p + 8 + 12L * nv > ge) throw new InvalidDataException();
            for (int k = 0; k < nv * 3; k++) Q(d, p + 8 + 4 * k, bits.Vertices);
            p += 8 + 12 * nv;
            Expect(d, p, "NRMS"); int nn = BitConverter.ToInt32(d, p + 4);
            if (p + 8 + 12L * nn > ge) throw new InvalidDataException();
            for (int k = 0; k < nn * 3; k++) Q(d, p + 8 + 4 * k, bits.Normals);
            st.Normals += nn;
            p += 8 + 12 * nn;
            if (bits.Uv < 23 || bits.Extents < 23)
            {
                p = Skip(d, p, ge, "PTYP", 4); p = Skip(d, p, ge, "PCNT", 4); p = Skip(d, p, ge, "PVTX", 2);
                p = Skip(d, p, ge, "GNDX", 1); p = Skip(d, p, ge, "MTGC", 4); p = Skip(d, p, ge, "MATS", 4);
                p += 12;
                for (int k = 0; k < 7; k++) Q(d, p + 4 * k, bits.Extents);
                p += 28;
                int ne = BitConverter.ToInt32(d, p); p += 4;
                if (ne < 0 || p + 28L * ne > ge) throw new InvalidDataException();
                for (int k = 0; k < ne * 7; k++) Q(d, p + 4 * k, bits.Extents);
                p += 28 * ne;
                Expect(d, p, "UVAS"); int nu = BitConverter.ToInt32(d, p + 4); p += 8;
                for (int u = 0; u < nu; u++)
                {
                    Expect(d, p, "UVBS"); int nt = BitConverter.ToInt32(d, p + 4);
                    if (p + 8 + 8L * nt > ge) throw new InvalidDataException();
                    for (int k = 0; k < nt * 2; k++) Q(d, p + 8 + 4 * k, bits.Uv);
                    p += 8 + 8 * nt;
                }
                if (p != ge) throw new InvalidDataException();
            }
            g = ge;
        }
    }

    static int Skip(byte[] d, int p, int ge, string tag, int elem)
    {
        Expect(d, p, tag);
        long n = BitConverter.ToUInt32(d, p + 4);
        long q = p + 8 + n * elem;
        if (q > ge) throw new InvalidDataException();
        return (int)q;
    }

    static void Expect(byte[] d, int p, string tag)
    {
        if (p + 8 > d.Length || Encoding.ASCII.GetString(d, p, 4) != tag) throw new InvalidDataException();
    }

    static int Node(byte[] d, int p, int e, Bits bits, Stats st)
    {
        int nsize = BitConverter.ToInt32(d, p);
        int ne = p + nsize;
        if (nsize < 96 || ne > e) throw new InvalidDataException();
        int q = p + 96;
        while (q < ne)
        {
            string tag = Encoding.ASCII.GetString(d, q, 4);
            int dim, keep;
            switch (tag)
            {
                case "KGTR": dim = 3; keep = bits.Translation; break;
                case "KGRT": dim = 4; keep = bits.Rotation; break;
                case "KGSC": dim = 3; keep = bits.Scaling; break;
                default: throw new InvalidDataException();
            }
            int count = BitConverter.ToInt32(d, q + 4), interp = BitConverter.ToInt32(d, q + 8);
            int vals = dim * (interp > 1 ? 3 : 1), keySize = 4 + 4 * vals;
            int k0 = q + 16;
            if (count < 0 || k0 + (long)count * keySize > ne) throw new InvalidDataException();
            for (int k = 0; k < count; k++)
                for (int v = 0; v < vals; v++)
                    Q(d, k0 + k * keySize + 4 + 4 * v, keep);
            st.Keys += count;
            q = k0 + count * keySize;
        }
        if (q != ne) throw new InvalidDataException();
        return ne;
    }
}
