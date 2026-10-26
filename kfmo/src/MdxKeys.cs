using System.Text;

namespace KFMO;

static class MdxKeys
{
    public sealed class Stats { public long Before, After; }

    const double TolTranslation = 0.01, TolRotation = 0.0005, TolScaling = 0.0005;

    public static byte[]? Apply(byte[] d, Stats st)
    {
        if (d.Length < 8 || Encoding.ASCII.GetString(d, 0, 4) != "MDLX") return null;
        try
        {
            var seqs = new List<(uint S, uint E)>();
            var glbs = new List<uint>();
            for (int i = 4; i + 8 <= d.Length;)
            {
                string tag = Encoding.ASCII.GetString(d, i, 4);
                int size = BitConverter.ToInt32(d, i + 4);
                if (size < 0 || i + 8L + size > d.Length) return null;
                if (tag == "SEQS") for (int p = i + 8; p + 132 <= i + 8 + size; p += 132) seqs.Add((BitConverter.ToUInt32(d, p + 80), BitConverter.ToUInt32(d, p + 84)));
                if (tag == "GLBS") for (int p = i + 8; p + 4 <= i + 8 + size; p += 4) glbs.Add(BitConverter.ToUInt32(d, p));
                i += 8 + size;
            }
            var outp = new MemoryStream(d.Length);
            outp.Write(d, 0, 4);
            long b0 = st.Before, a0 = st.After;
            for (int i = 4; i + 8 <= d.Length;)
            {
                string tag = Encoding.ASCII.GetString(d, i, 4);
                int size = BitConverter.ToInt32(d, i + 4), s = i + 8, e = s + size;
                if (tag == "BONE" || tag == "HELP")
                {
                    var body = new MemoryStream();
                    for (int p = s; p < e;)
                    {
                        int ne = p + BitConverter.ToInt32(d, p);
                        if (ne > e || ne < p + 96) throw new InvalidDataException();
                        var node = Node(d, p, ne, seqs, glbs, st);
                        body.Write(node);
                        p = ne;
                        if (tag == "BONE") { body.Write(d, p, 8); p += 8; }
                    }
                    outp.Write(d, i, 4);
                    outp.Write(BitConverter.GetBytes((int)body.Length));
                    body.Position = 0; body.CopyTo(outp);
                }
                else outp.Write(d, i, 8 + size);
                i = e;
            }
            if (st.Before - b0 == st.After - a0) return null;
            return outp.ToArray();
        }
        catch { return null; }
    }

    static byte[] Node(byte[] d, int p, int ne, List<(uint S, uint E)> seqs, List<uint> glbs, Stats st)
    {
        var ms = new MemoryStream();
        ms.Write(d, p, 96);
        int q = p + 96;
        while (q < ne)
        {
            string tag = Encoding.ASCII.GetString(d, q, 4);
            int dim = tag switch { "KGTR" => 3, "KGRT" => 4, "KGSC" => 3, _ => throw new InvalidDataException() };
            int count = BitConverter.ToInt32(d, q + 4), interp = BitConverter.ToInt32(d, q + 8), gs = BitConverter.ToInt32(d, q + 12);
            int vals = dim * (interp > 1 ? 3 : 1), keySize = 4 + 4 * vals;
            int k0 = q + 16;
            if (count < 0 || k0 + (long)count * keySize > ne) throw new InvalidDataException();
            var keep = Reduce(d, k0, count, keySize, dim, interp, gs, tag, seqs, glbs);
            int kept = keep.Count(x => x);
            st.Before += count; st.After += kept;
            ms.Write(d, q, 4);
            ms.Write(BitConverter.GetBytes(kept));
            ms.Write(d, q + 8, 8);
            for (int k = 0; k < count; k++) if (keep[k]) ms.Write(d, k0 + k * keySize, keySize);
            q = k0 + count * keySize;
        }
        if (q != ne) throw new InvalidDataException();
        var res = ms.ToArray();
        BitConverter.TryWriteBytes(res.AsSpan(0, 4), res.Length);
        return res;
    }

    static bool[] Reduce(byte[] d, int k0, int count, int keySize, int dim, int interp, int gs,
                         string tag, List<(uint S, uint E)> seqs, List<uint> glbs)
    {
        var keep = Enumerable.Repeat(true, count).ToArray();
        if (count < 3 || interp > 1) return keep;
        uint T(int k) => BitConverter.ToUInt32(d, k0 + k * keySize);
        float V(int k, int c) => BitConverter.ToSingle(d, k0 + k * keySize + 4 + 4 * c);
        for (int k = 1; k < count; k++) if (T(k) <= T(k - 1)) return keep;

        string SeqKey(int k)
        {
            uint t = T(k);
            if (gs >= 0) return gs < glbs.Count && t <= glbs[gs] ? "g" : "";
            var sb = new StringBuilder();
            for (int i = 0; i < seqs.Count; i++) if (t >= seqs[i].S && t <= seqs[i].E) sb.Append(i).Append(',');
            return sb.ToString();
        }
        var key = new string[count];
        for (int k = 0; k < count; k++) key[k] = SeqKey(k);

        double tol = tag == "KGTR" ? TolTranslation : tag == "KGRT" ? TolRotation : TolScaling;
        int a = 0;
        while (a < count)
        {
            int b = a;
            while (b + 1 < count && key[b + 1] == key[a]) b++;
            if (key[a].Length > 0 && b - a >= 2)
            {
                int anchor = a;
                int j = a + 2;
                while (j <= b)
                {
                    bool ok = true;
                    for (int m = anchor + 1; m < j && ok; m++) ok = Fits(m, anchor, j);
                    if (ok) { j++; continue; }
                    for (int m = anchor + 1; m < j - 1; m++) keep[m] = false;
                    anchor = j - 1;
                    j = anchor + 2;
                }
                for (int m = anchor + 1; m < b; m++) keep[m] = false;
            }
            a = b + 1;
        }
        return keep;

        bool Fits(int m, int x, int y)
        {
            if (interp == 0)
            {
                for (int c = 0; c < dim; c++) if (V(m, c) != V(x, c)) return false;
                return true;
            }
            double t = (double)(T(m) - T(x)) / (T(y) - T(x));
            if (dim == 4)
            {
                var r = Slerp(x, y, t);
                double dot = 0; for (int c = 0; c < 4; c++) dot += r[c] * V(m, c);
                double sgn = dot < 0 ? -1 : 1;
                for (int c = 0; c < 4; c++) if (Math.Abs(r[c] * sgn - V(m, c)) > Math.Max(tol, Math.Abs(V(m, c)) / 512)) return false;
                return true;
            }
            for (int c = 0; c < dim; c++)
            {
                double v = V(x, c) + (V(y, c) - V(x, c)) * t;
                double lim = Math.Max(tag == "KGSC" ? tol * Math.Max(1, Math.Abs(V(m, c))) : tol, Math.Abs(V(m, c)) / 512);
                if (Math.Abs(v - V(m, c)) > lim) return false;
            }
            return true;
        }

        double[] Slerp(int x, int y, double t)
        {
            double[] qa = new double[4], qb = new double[4];
            double dot = 0;
            for (int c = 0; c < 4; c++) { qa[c] = V(x, c); qb[c] = V(y, c); dot += qa[c] * qb[c]; }
            if (dot < 0) { dot = -dot; for (int c = 0; c < 4; c++) qb[c] = -qb[c]; }
            double wa, wb;
            if (dot > 0.9995) { wa = 1 - t; wb = t; }
            else
            {
                double th = Math.Acos(Math.Min(1, dot)), sn = Math.Sin(th);
                wa = Math.Sin((1 - t) * th) / sn; wb = Math.Sin(t * th) / sn;
            }
            var r = new double[4]; double n = 0;
            for (int c = 0; c < 4; c++) { r[c] = wa * qa[c] + wb * qb[c]; n += r[c] * r[c]; }
            n = Math.Sqrt(n);
            for (int c = 0; c < 4; c++) r[c] /= n;
            return r;
        }
    }
}
