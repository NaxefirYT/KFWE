using System.Text;
using System.Text.RegularExpressions;

namespace KFMO;

static class Strings
{
    static readonly string[] ObjectFiles = { "war3map.w3u", "war3map.w3t", "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3h", "war3map.w3q" };

    static readonly Regex Entry = new(@"STRING (\d+)[^\n]*\n(?:[ \t]*//[^\n]*\n)*[ \t]*\{\r?\n(.*?)\r?\n\}", RegexOptions.Singleline | RegexOptions.Compiled);

    public sealed class Result { public int Before, After, Inlined; public long BytesBefore, BytesAfter; }

    public static Dictionary<int, string> ParseWts(byte[] wtsBytes)
    {
        bool bom = wtsBytes.Length >= 3 && wtsBytes[0] == 0xEF && wtsBytes[1] == 0xBB && wtsBytes[2] == 0xBF;
        string wts = Encoding.UTF8.GetString(wtsBytes, bom ? 3 : 0, wtsBytes.Length - (bom ? 3 : 0));
        var entries = new Dictionary<int, string>();
        foreach (Match m in Entry.Matches(wts))
            entries[int.Parse(m.Groups[1].Value)] = m.Groups[2].Value;
        return entries;
    }

    public static byte[] AppendWts(byte[]? wtsBytes, IEnumerable<(int Id, string Text)> add)
    {
        var sb = new StringBuilder();
        foreach (var (id, text) in add) sb.Append("STRING ").Append(id).Append("\r\n{\r\n").Append(text).Append("\r\n}\r\n\r\n");
        var nb = Encoding.UTF8.GetBytes(sb.ToString());
        return wtsBytes == null ? nb : [.. wtsBytes, .. nb];
    }

    public static Result? Run(List<(string Name, byte[] Data)> files)
    {
        int wi = files.FindIndex(f => f.Name.Equals("war3map.wts", StringComparison.OrdinalIgnoreCase));
        if (wi < 0) return null;
        var wtsBytes = files[wi].Data;
        bool bom = wtsBytes.Length >= 3 && wtsBytes[0] == 0xEF && wtsBytes[1] == 0xBB && wtsBytes[2] == 0xBF;
        string wts = Encoding.UTF8.GetString(wtsBytes, bom ? 3 : 0, wtsBytes.Length - (bom ? 3 : 0));
        var entries = new Dictionary<int, string>();
        foreach (Match m in Entry.Matches(wts))
            entries[int.Parse(m.Groups[1].Value)] = m.Groups[2].Value;
        var res = new Result { Before = entries.Count, BytesBefore = wtsBytes.Length };

        for (int i = 0; i < files.Count; i++)
        {
            if (!ObjectFiles.Contains(files[i].Name, StringComparer.OrdinalIgnoreCase)) continue;
            bool levels = files[i].Name.EndsWith(".w3a", StringComparison.OrdinalIgnoreCase)
                       || files[i].Name.EndsWith(".w3d", StringComparison.OrdinalIgnoreCase)
                       || files[i].Name.EndsWith(".w3q", StringComparison.OrdinalIgnoreCase);
            var rewritten = InlineObjectFile(files[i].Data, levels, entries, out int inl);
            if (rewritten != null && inl > 0) { files[i] = (files[i].Name, rewritten); res.Inlined += inl; }
        }

        var used = new HashSet<int>();
        var re = new Regex(@"TRIGSTR_0*(\d+)", RegexOptions.Compiled);
        foreach (var f in files)
        {
            if (f.Name.Equals("war3map.wts", StringComparison.OrdinalIgnoreCase)) continue;
            string ext = Path.GetExtension(f.Name).ToLowerInvariant();
            if (ext is ".mdx" or ".blp" or ".tga" or ".wav" or ".mp3" or ".mdl" or ".dds") continue;
            foreach (Match m in re.Matches(Encoding.Latin1.GetString(f.Data)))
                used.Add(int.Parse(m.Groups[1].Value));
        }
        var sb = new StringBuilder();
        foreach (var (id, text) in entries.OrderBy(e => e.Key))
            if (used.Contains(id))
                sb.Append("STRING ").Append(id).Append("\r\n{\r\n").Append(text).Append("\r\n}\r\n\r\n");
        var nb = Encoding.UTF8.GetBytes(sb.ToString());
        files[wi] = ("war3map.wts", bom ? [0xEF, 0xBB, 0xBF, .. nb] : nb);
        res.After = used.Count(entries.ContainsKey);
        res.BytesAfter = files[wi].Data.Length;
        return res;
    }

    static byte[]? InlineObjectFile(byte[] d, bool levels, Dictionary<int, string> entries, out int inlined)
    {
        inlined = 0;
        try
        {
            var r = new BinaryReader(new MemoryStream(d));
            var outp = new MemoryStream(d.Length * 2);
            var w = new BinaryWriter(outp);
            int version = r.ReadInt32();
            if (version != 1 && version != 2) return null;
            w.Write(version);
            for (int table = 0; table < 2; table++)
            {
                int objects = r.ReadInt32();
                w.Write(objects);
                for (int o = 0; o < objects; o++)
                {
                    w.Write(r.ReadBytes(8));
                    int mods = r.ReadInt32();
                    w.Write(mods);
                    for (int m = 0; m < mods; m++)
                    {
                        w.Write(r.ReadBytes(4));
                        int type = r.ReadInt32();
                        w.Write(type);
                        if (levels) w.Write(r.ReadBytes(8));
                        if (type == 3)
                        {
                            var sb = new List<byte>();
                            byte b;
                            while ((b = r.ReadByte()) != 0) sb.Add(b);
                            var raw = sb.ToArray();
                            if (TryTrig(raw, out int id) && entries.TryGetValue(id, out var text) && Encoding.UTF8.GetByteCount(text) <= 1023)
                            {
                                w.Write(Encoding.UTF8.GetBytes(text));
                                inlined++;
                            }
                            else w.Write(raw);
                            w.Write((byte)0);
                        }
                        else if (type is 0 or 1 or 2) w.Write(r.ReadBytes(4));
                        else return null;
                        w.Write(r.ReadBytes(4));
                    }
                }
            }
            if (r.BaseStream.Position != d.Length) return null;
            return outp.ToArray();
        }
        catch (EndOfStreamException) { return null; }
    }

    static bool TryTrig(byte[] s, out int id)
    {
        id = 0;
        const string P = "TRIGSTR_";
        if (s.Length <= P.Length || Encoding.ASCII.GetString(s, 0, P.Length) != P) return false;
        for (int k = P.Length; k < s.Length; k++)
        {
            if (s[k] < '0' || s[k] > '9') return false;
            id = id * 10 + (s[k] - '0');
        }
        return true;
    }
}
