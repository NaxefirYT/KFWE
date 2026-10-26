using War3Net.IO.Mpq;

namespace KFMO;

static class MdxDiff
{
    static byte[] Read(string map, string name, IEnumerable<string> names)
    {
        using var a = MpqArchive.Open(map, true);
        a.AddFileNames(names);
        using var s = a.OpenFile(name); using var ms = new MemoryStream(); s.CopyTo(ms); return ms.ToArray();
    }

    public static void Run(string mapA, string mapB, string name, string namesFrom)
    {
        var names = Analyzer.ListFile(namesFrom);
        var a = Read(mapA, name, names); var b = Read(mapB, name, names);
        Console.WriteLine($"{name}: {a.Length} -> {b.Length} байт");
        if (a.Length != b.Length) return;
        int words = a.Length / 4, changed = 0;
        double maxAbs = 0, maxRel = 0;
        var zeroBits = new int[24];
        for (int i = 0; i < words; i++)
        {
            uint x = BitConverter.ToUInt32(a, i * 4), y = BitConverter.ToUInt32(b, i * 4);
            if (x == y) continue;
            changed++;
            float fx = BitConverter.ToSingle(a, i * 4), fy = BitConverter.ToSingle(b, i * 4);
            double abs = Math.Abs(fx - fy), rel = abs / Math.Max(1e-9, Math.Abs(fx));
            if (!double.IsNaN(abs)) { maxAbs = Math.Max(maxAbs, abs); maxRel = Math.Max(maxRel, rel); }
            int tz = 0; while (tz < 23 && ((y >> tz) & 1) == 0) tz++;
            zeroBits[Math.Min(23, tz)]++;
        }
        Console.WriteLine($"изменено слов: {changed} из {words}, макс. абс. {maxAbs:G4}, макс. отн. {maxRel:G4}");
        var perTag = new Dictionary<string, (int changed, int total)>();
        for (int i = 4; i + 8 <= a.Length;)
        {
            string tag = System.Text.Encoding.ASCII.GetString(a, i, 4); int sz = BitConverter.ToInt32(a, i + 4);
            int ch = 0; for (int k = i + 8; k + 4 <= i + 8 + sz; k += 4) if (BitConverter.ToUInt32(a, k) != BitConverter.ToUInt32(b, k)) ch++;
            var cur = perTag.GetValueOrDefault(tag); perTag[tag] = (cur.changed + ch, cur.total + sz / 4);
            i += 8 + sz;
        }
        foreach (var kv in perTag.Where(k => k.Value.changed > 0)) Console.WriteLine($"  {kv.Key}: изменено {kv.Value.changed} из {kv.Value.total}");
        int shown = 0;
        for (int i = 0; i + 4 <= a.Length && shown < 12; i += 4)
            if (BitConverter.ToUInt32(a, i) != BitConverter.ToUInt32(b, i)) { Console.WriteLine($"   @{i}: {BitConverter.ToUInt32(a, i):X8} ({BitConverter.ToSingle(a, i):G7}) -> {BitConverter.ToUInt32(b, i):X8} ({BitConverter.ToSingle(b, i):G7})"); shown++; }
        Console.WriteLine("младших нулевых бит у изменённых: " + string.Join(" ", Enumerable.Range(0, 24).Where(i => zeroBits[i] > 0).Select(i => $"{i}:{zeroBits[i]}")));
    }
}
