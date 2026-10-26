using System.Globalization;
using System.Reflection;
using System.Text;

namespace KFMO;

static class LV
{
    static readonly CultureInfo Inv = CultureInfo.InvariantCulture;

    public static bool IsNum(object? v) => v is long or double;

    public static bool Eq(object? a, object? b)
    {
        if (a == null || b == null) return a == null && b == null;
        if (a is long la && b is long lb) return la == lb;
        if (IsNum(a) && IsNum(b)) return ToD(a) == ToD(b);
        if (a is string sa && b is string sb) return string.Equals(sa, sb, StringComparison.Ordinal);
        return false;
    }

    static double ToD(object v) => v is long l ? l : (double)v;

    public static string ToStr(object? v)
    {
        switch (v)
        {
            case null: return "nil";
            case string s: return s;
            case long l: return l.ToString(Inv);
            case double d:
                if (double.IsNaN(d)) return d < 0 ? "-nan" : "nan";
                if (double.IsInfinity(d)) return d < 0 ? "-inf" : "inf";
                var g = d.ToString("G14", Inv).Replace("E+", "e+").Replace("E-", "e-");
                int e = g.IndexOf('e');
                if (e >= 0 && g.Length - e - 2 < 2) g = g.Substring(0, e + 2) + "0" + g.Substring(e + 2);
                if (g.IndexOfAny(new[] { '.', 'e', 'n', 'i' }) < 0) g += ".0";
                return g;
            default: return v.ToString() ?? "";
        }
    }

    public static object? StrToNumber(string s)
    {
        var t = s.Trim(' ', '\t', '\n', '\r', '\f', '\v');
        if (t.Length == 0) return null;
        bool neg = false; int p = 0;
        if (t[0] == '-' || t[0] == '+') { neg = t[0] == '-'; p = 1; }
        if (t.Length - p > 2 && t[p] == '0' && (t[p + 1] == 'x' || t[p + 1] == 'X'))
        {
            ulong acc = 0; bool any = false;
            for (int i = p + 2; i < t.Length; i++)
            {
                int h = HexVal(t[i]);
                if (h < 0) { any = false; break; }
                acc = acc * 16 + (ulong)h; any = true;
            }
            if (any) return neg ? -(long)acc : (long)acc;
            return null;
        }
        bool digits = t.Length > p;
        for (int i = p; i < t.Length; i++) if (t[i] < '0' || t[i] > '9') { digits = false; break; }
        if (digits && long.TryParse(t, NumberStyles.AllowLeadingSign, Inv, out long lv)) return lv;
        if (t.IndexOfAny(new[] { 'n', 'N', 'i', 'I' }) >= 0) return null;
        if (double.TryParse(t, NumberStyles.Float, Inv, out double d)) return d;
        return null;
    }

    static int HexVal(char c) => c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;

    public static object WToNumber(object? v)
    {
        string z = ToStr(v);
        int p = 0;
        while (p < z.Length && (z[p] == ' ' || z[p] == '\t')) p++;
        if (p >= z.Length || z[p] == '\n' || z[p] == '\r') return 0L;
        if (z[p] == '\'')
        {
            p++;
            int start = p; int n = 0;
            for (; p - start <= 4; p++)
            {
                if (p >= z.Length || z[p] == '\n' || z[p] == '\r' || z[p] == '\'') return (long)n;
                n = (n << 8) + (z[p] & 0xFF);
            }
            return 0L;
        }
        int end = StrToL(z, p, out long i, out bool overflow);
        if (overflow) return 0L;
        if (end >= z.Length || z[end] != '.') return i;
        double d = StrToD(z, p);
        if (double.IsInfinity(d)) return 0.0;
        return d;
    }

    static int StrToL(string z, int p, out long val, out bool overflow)
    {
        val = 0; overflow = false;
        int s = p;
        while (s < z.Length && char.IsWhiteSpace(z[s])) s++;
        bool neg = false;
        if (s < z.Length && (z[s] == '+' || z[s] == '-')) { neg = z[s] == '-'; s++; }
        int bas = 10;
        if (s < z.Length && z[s] == '0')
        {
            if (s + 1 < z.Length && (z[s + 1] == 'x' || z[s + 1] == 'X') && s + 2 < z.Length && HexVal(z[s + 2]) >= 0) { bas = 16; s += 2; }
            else bas = 8;
        }
        int q = s; long acc = 0;
        for (; q < z.Length; q++)
        {
            int dv = HexVal(z[q]);
            if (dv < 0 || dv >= bas) break;
            acc = acc * bas + dv;
            if (acc > 0x80000000L) { overflow = true; acc = 0x80000001L; }
        }
        if (q == s && bas != 8) return p;
        if (neg) acc = -acc;
        if (acc > int.MaxValue || acc < int.MinValue) overflow = true;
        val = acc;
        return q;
    }

    public static double StrToD(string z, int p)
    {
        int s = p;
        while (s < z.Length && char.IsWhiteSpace(z[s])) s++;
        int q = s;
        if (q < z.Length && (z[q] == '+' || z[q] == '-')) q++;
        int ds = q;
        while (q < z.Length && char.IsAsciiDigit(z[q])) q++;
        if (q < z.Length && z[q] == '.') { q++; while (q < z.Length && char.IsAsciiDigit(z[q])) q++; }
        if (q == ds || (q == ds + 1 && z[ds] == '.')) return 0;
        if (q < z.Length && (z[q] == 'e' || z[q] == 'E'))
        {
            int r = q + 1;
            if (r < z.Length && (z[r] == '+' || z[r] == '-')) r++;
            if (r < z.Length && char.IsAsciiDigit(z[r])) { while (r < z.Length && char.IsAsciiDigit(z[r])) r++; q = r; }
        }
        return double.Parse(z.AsSpan(s, q - s), NumberStyles.Float, Inv);
    }

    public static long Floor(object? v)
    {
        if (v is long l) return l;
        if (v is double d) return (long)Math.Floor(d);
        if (v is string s && StrToNumber(s) is { } n) return Floor(n);
        return 0;
    }

    static readonly int[] RealPrec = { 1, 2, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

    public static string ConvertReal(double v)
    {
        foreach (int p in RealPrec)
        {
            var g = v.ToString("F" + p, Inv);
            if (double.Parse(g, Inv) == v) return g;
        }
        return v.ToString("F17", Inv);
    }

    public static string Bin2Float(float f)
    {
        var s = ((double)f).ToString("F6", Inv);
        return s.TrimEnd('0');
    }

    public static float Float2Bin(object? v)
    {
        var z = ToStr(v);
        return (float)StrToD(z, 0);
    }
}

sealed class LTable : Dictionary<int, object?>
{
    public LTable() { }
    public LTable(LTable o) : base(o) { }

    public int Len()
    {
        int n = 0;
        while (TryGetValue(n + 1, out var v) && v != null) n++;
        return n;
    }

    public int MaxIndex()
    {
        int m = 0;
        foreach (var (k, v) in this) if (v != null && k > m) m = k;
        return m;
    }

    public object? At(int i) => TryGetValue(i, out var v) ? v : null;
}

sealed class Meta
{
    public string Key = "";
    public string Id = "";
    public string Field = "";
    public string MKey = "";
    public int Type;
    public int? Repeat, Index;
    public int Data;
    public bool Profile, Concat, AppendIndex, CantEmpty, Splite;
    public List<string>? Default;
}

static class MetaDb
{
    static Dictionary<string, Dictionary<string, Meta>>? meta;
    static Dictionary<string, List<string>>? keydata;

    public static string Res(string name)
    {
        using var s = Assembly.GetExecutingAssembly().GetManifestResourceStream(name) ?? throw new FileNotFoundException(name);
        using var r = new StreamReader(s, Encoding.UTF8);
        return r.ReadToEnd();
    }

    public static Dictionary<string, Dictionary<string, Meta>> Metadata => meta ??= LoadMeta();
    public static Dictionary<string, List<string>> KeyData => keydata ??= LoadKeyData();

    public static Dictionary<string, Meta>? Of(string? section) =>
        section != null && Metadata.TryGetValue(section, out var m) ? m : null;

    static Dictionary<string, Dictionary<string, Meta>> LoadMeta()
    {
        var res = new Dictionary<string, Dictionary<string, Meta>>(StringComparer.Ordinal);
        Dictionary<string, Meta>? cur = null;
        Meta? m = null;
        foreach (var raw in Res("metadata.ini").Split('\n'))
        {
            var line = raw.TrimEnd('\r').Trim();
            if (line.Length == 0) continue;
            if (line.StartsWith("[."))
            {
                m = new Meta { Key = line[2..^1] };
                cur![m.Key] = m;
                continue;
            }
            if (line.StartsWith("["))
            {
                cur = new Dictionary<string, Meta>(StringComparer.Ordinal);
                res[line[1..^1]] = cur;
                m = null;
                continue;
            }
            int eq = line.IndexOf('=');
            if (eq < 0 || m == null) continue;
            var k = line[..eq].Trim(); var v = line[(eq + 1)..].Trim();
            switch (k)
            {
                case "id": m.Id = v; break;
                case "field": m.Field = v; break;
                case "key": m.MKey = v; break;
                case "type": m.Type = int.Parse(v); break;
                case "repeat": m.Repeat = int.Parse(v); break;
                case "index": m.Index = int.Parse(v); break;
                case "data": m.Data = int.Parse(v); break;
                case "profile": m.Profile = v == "true"; break;
                case "concat": m.Concat = v == "true"; break;
                case "appendindex": m.AppendIndex = v == "true"; break;
                case "cantempty": m.CantEmpty = v == "true"; break;
                case "splite": m.Splite = v == "true"; break;
                case "default": m.Default = v.Trim('{', '}').Split(',').Select(x => x.Trim()).ToList(); break;
            }
        }
        return res;
    }

    static Dictionary<string, List<string>> LoadKeyData()
    {
        var res = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        List<string>? cur = null;
        foreach (var raw in Res("keydata.ini").Split('\n'))
        {
            var line = raw.TrimEnd('\r').Trim();
            if (line.Length == 0) continue;
            if (line.EndsWith("= {"))
            {
                var name = line[..^3].Trim();
                if (name.StartsWith('"')) name = name[1..^1].Replace("\\\\", "\\");
                cur = new List<string>();
                res[name] = cur;
            }
            else if (line == "}") cur = null;
            else cur?.Add(line.TrimEnd(','));
        }
        return res;
    }
}

static class W3Txt
{
    public static void Parse(byte[] data, Dictionary<string, Dictionary<string, List<string>>> into)
    {
        int start = data.Length >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF ? 3 : 0;
        string z = Encoding.UTF8.GetString(data, start, data.Length - start);
        Dictionary<string, List<string>>? sec = null;
        int i = 0, n = z.Length;
        while (i < n)
        {
            int e = i;
            while (e < n && z[e] != '\n' && z[e] != '\r') e++;
            ParseLine(z, i, e, into, ref sec);
            i = e;
            if (i < n) { char old = z[i]; i++; if (i < n && (z[i] == '\n' || z[i] == '\r') && z[i] != old) i++; }
        }
    }

    static void ParseLine(string z, int p, int e, Dictionary<string, Dictionary<string, List<string>>> into, ref Dictionary<string, List<string>>? sec)
    {
        if (p >= e) { if (sec != null) { } return; }
        if (z[p] == '[')
        {
            int c = z.IndexOf(']', p + 1, e - p - 1);
            if (c < 0) return;
            var name = z.Substring(p + 1, c - p - 1).ToLowerInvariant();
            if (!into.TryGetValue(name, out sec)) { sec = new Dictionary<string, List<string>>(StringComparer.Ordinal); into[name] = sec; }
            return;
        }
        if (z[p] == '/' && p + 1 < e && z[p + 1] == '/') return;
        int eq = z.IndexOf('=', p, e - p);
        if (eq < 0) return;
        if (sec == null) return;
        var key = z.Substring(p, eq - p).ToLowerInvariant();
        if (!sec.TryGetValue(key, out var vals)) { vals = new List<string>(); sec[key] = vals; }
        ParseValue(z, eq + 1, e, vals);
    }

    static void ParseValue(string z, int q, int e, List<string> vals)
    {
        int p = q;
        bool hasEmpty = true;
        while (true)
        {
            if (q >= e)
            {
                if (q - p > 0) vals.Add(z.Substring(p, q - p));
                return;
            }
            char c = z[q];
            if (c == '"')
            {
                q++;
                int s = q;
                while (q < e && z[q] != '"') q++;
                vals.Add(z.Substring(s, q - s));
                if (q >= e) return;
                p = q + 1; hasEmpty = false; q++;
            }
            else if (c == ',')
            {
                if (hasEmpty || q - p > 0) vals.Add(z.Substring(p, q - p));
                p = q + 1; hasEmpty = true; q++;
            }
            else
            {
                int s = q;
                while (true)
                {
                    if (q >= e) { vals.Add(z.Substring(s, q - s)); return; }
                    if (z[q] == ',') { vals.Add(z.Substring(s, q - s)); break; }
                    if (z[q] == '/' && q + 1 < e && z[q + 1] == '/') { vals.Add(z.Substring(s, q - s)); return; }
                    q++;
                }
                p = q + 1; q++;
            }
        }
    }
}

static class W3Slk
{
    public static Dictionary<string, Dictionary<string, object?>> Parse(byte[] data)
    {
        var res = new Dictionary<string, Dictionary<string, object?>>(StringComparer.Ordinal);
        string z = Encoding.UTF8.GetString(data);
        int maxx = 0, maxy = 0, curx = 1, cury = 1;
        string[]? colName = null, rowId = null;
        string?[,]? cells = null;
        foreach (var raw in z.Split('\n'))
        {
            var line = raw.TrimEnd('\r');
            if (line.Length == 0) continue;
            char kind = line[0];
            if (kind == 'E' && line.Length == 1) break;
            if (kind == 'B')
            {
                foreach (var part in line.Split(';').Skip(1))
                {
                    var t = part.Trim();
                    if (t.StartsWith('X')) int.TryParse(t.AsSpan(1).Trim(), out maxx);
                    else if (t.StartsWith('Y')) int.TryParse(t.AsSpan(1).Trim(), out maxy);
                }
                colName = new string[maxx];
                rowId = new string[maxy];
                cells = new string?[maxx, maxy];
                continue;
            }
            if (kind != 'C' && kind != 'F') continue;
            if (cells == null) continue;
            int i = 1;
            while (i < line.Length)
            {
                while (i < line.Length && (line[i] == ' ' || line[i] == '\t')) i++;
                if (i >= line.Length || line[i] != ';') break;
                i++;
                while (i < line.Length && (line[i] == ' ' || line[i] == '\t')) i++;
                if (i >= line.Length) break;
                char f = line[i];
                if (f == 'X' || f == 'Y')
                {
                    i++;
                    int s = i;
                    while (i < line.Length && char.IsAsciiDigit(line[i])) i++;
                    int.TryParse(line.AsSpan(s, i - s), out int v);
                    if (f == 'X') curx = v; else cury = v;
                }
                else if (f == 'K')
                {
                    i++;
                    while (i < line.Length && (line[i] == ' ' || line[i] == '\t')) i++;
                    int s = i, q = i;
                    if (q < line.Length && line[q] == '"') { q++; while (q < line.Length && line[q] != '"') q++; }
                    while (q < line.Length && line[q] != ';') q++;
                    var val = line.Substring(s, q - s).TrimEnd(' ', '\t');
                    i = q;
                    if (curx < 1 || cury < 1 || curx > maxx || cury > maxy) continue;
                    if (curx == 1) { if (cury != 1) rowId![cury - 1] = val; }
                    else if (cury == 1) colName![curx - 1] = StripQ(val, out _);
                    else cells[curx - 1, cury - 1] = val;
                }
                else
                {
                    while (i < line.Length && line[i] != ';') i++;
                }
            }
        }
        if (cells == null) return res;
        for (int y = 1; y < maxy; y++)
        {
            var id = rowId![y];
            if (string.IsNullOrEmpty(id)) continue;
            id = StripQ(id, out _);
            if (id.Length == 0) continue;
            var row = new Dictionary<string, object?>(StringComparer.Ordinal);
            for (int x = 1; x < maxx; x++)
            {
                var cn = colName![x];
                if (string.IsNullOrEmpty(cn)) continue;
                var raw = cells[x, y];
                object? v = null;
                if (raw != null)
                {
                    var s = StripQ(raw, out bool str);
                    if (str) v = s;
                    else if (s.Length > 0) v = LV.StrToNumber(s) ?? 0L;
                }
                row[cn] = v;
            }
            res[id] = row;
        }
        return res;
    }

    static string StripQ(string s, out bool isStr)
    {
        isStr = false;
        if (s.Length > 0 && s[0] == '"') { s = s[1..]; isStr = true; }
        if (s.Length > 0 && s[^1] == '"') { s = s[..^1]; isStr = true; }
        return s;
    }
}
