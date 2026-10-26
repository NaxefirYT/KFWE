using System.Text;

namespace KFMO;

static class MdxClean
{
    public static bool Clean(byte[] d, out int zeroed)
    {
        zeroed = 0;
        if (d.Length < 8 || Encoding.ASCII.GetString(d, 0, 4) != "MDLX") return false;
        var copy = (byte[])d.Clone();
        int z = 0;
        try
        {
            int i = 4;
            while (i + 8 <= d.Length)
            {
                string tag = Encoding.ASCII.GetString(d, i, 4);
                int size = BitConverter.ToInt32(d, i + 4), s = i + 8, e = s + size;
                if (size < 0 || e > d.Length) throw new InvalidDataException();
                switch (tag)
                {
                    case "MODL": z += Str(d, s, 80) + Str(d, s + 80, 260); break;
                    case "SEQS": for (int p = s; p + 132 <= e; p += 132) z += Str(d, p, 80); break;
                    case "TEXS": for (int p = s; p + 268 <= e; p += 268) z += Str(d, p + 4, 260); break;
                    case "BONE": z += Nodes(d, s, e, 8); break;
                    case "HELP": z += Nodes(d, s, e, 0); break;
                    case "EVTS": z += Events(d, s, e); break;
                    case "LITE": case "PRE2": case "RIBB": case "CORN":
                        z += Objects(d, s, e, 0, -1, 0); break;
                    case "ATCH": z += Objects(d, s, e, 0, 0, 260); break;
                    case "PREM": z += Objects(d, s, e, 0, 16, 260); break;
                    case "CAMS":
                        for (int p = s; p < e;)
                        {
                            int osz = BitConverter.ToInt32(d, p);
                            if (osz <= 0 || p + osz > e) throw new InvalidDataException();
                            z += Str(d, p + 4, 80);
                            p += osz;
                        }
                        break;
                }
                i = e;
            }
            if (i != d.Length) throw new InvalidDataException();
        }
        catch
        {
            Buffer.BlockCopy(copy, 0, d, 0, d.Length);
            return false;
        }
        zeroed = z;
        return z > 0;
    }

    static int Str(byte[] d, int p, int len)
    {
        int nul = Array.IndexOf(d, (byte)0, p, len);
        if (nul < 0) return 0;
        int n = 0;
        for (int k = nul + 1; k < p + len; k++)
            if (d[k] != 0) { d[k] = 0; n++; }
        return n;
    }

    static int NodeAt(byte[] d, int p, int end, out int nodeSize)
    {
        nodeSize = BitConverter.ToInt32(d, p);
        if (nodeSize < 96 || p + nodeSize > end) throw new InvalidDataException();
        return Str(d, p + 4, 80);
    }

    static int Nodes(byte[] d, int s, int e, int tail)
    {
        int z = 0;
        for (int p = s; p < e;)
        {
            z += NodeAt(d, p, e, out int nsz);
            p += nsz + tail;
        }
        return z;
    }

    static int Events(byte[] d, int s, int e)
    {
        int z = 0;
        for (int p = s; p < e;)
        {
            z += NodeAt(d, p, e, out int nsz);
            p += nsz;
            if (p + 12 <= e && Encoding.ASCII.GetString(d, p, 4) == "KEVT")
            {
                int cnt = BitConverter.ToInt32(d, p + 4);
                if (cnt < 0) throw new InvalidDataException();
                p += 12 + 4 * cnt;
            }
        }
        return z;
    }

    static int Objects(byte[] d, int s, int e, int unused, int pathOffset, int pathLen)
    {
        int z = 0;
        for (int p = s; p < e;)
        {
            int osz = BitConverter.ToInt32(d, p);
            if (osz < 100 || p + osz > e) throw new InvalidDataException();
            z += NodeAt(d, p + 4, p + osz, out int nsz);
            if (pathOffset >= 0)
            {
                int pp = p + 4 + nsz + pathOffset;
                if (pp + pathLen > p + osz) throw new InvalidDataException();
                z += Str(d, pp, pathLen);
            }
            p += osz;
        }
        return z;
    }
}
