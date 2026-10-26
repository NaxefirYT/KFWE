using System.Security.Cryptography;
using System.Text;
using War3Net.IO.Mpq;

static class Analyzer
{
    public static IReadOnlyList<string> KnownNamesList => KnownNames.Concat(KFMO.StdNames.Extra).ToArray();
    static readonly string[] KnownNames =
    {
        "war3map.j", "war3map.lua", "scripts\\war3map.j", "war3map.wts", "war3map.w3i", "war3map.w3e", "war3map.shd",
        "war3map.wpm", "war3map.doo", "war3mapUnits.doo", "war3map.w3r", "war3map.w3c", "war3map.w3s", "war3map.mmp",
        "war3map.w3u", "war3map.w3t", "war3map.w3a", "war3map.w3b", "war3map.w3d", "war3map.w3q", "war3map.w3h",
        "war3map.wtg", "war3map.wct", "war3map.imp", "war3mapMap.blp", "war3mapMap.b00", "war3mapMap.tga",
        "war3mapPreview.tga", "war3mapPath.tga", "war3mapMisc.txt", "war3mapSkin.txt", "war3mapExtra.txt",
        "(listfile)", "(attributes)", "(signature)", "conversation.json",
        "units\\unitdata.slk", "units\\abilitydata.slk", "units\\itemdata.slk", "units\\upgradedata.slk",
        "units\\destructabledata.slk", "doodads\\doodads.slk", "units\\abilitybuffdata.slk", "units\\unitui.slk",
        "units\\unitbalance.slk", "units\\unitabilities.slk", "units\\unitweapons.slk",
    };

    public static void Run(string path, string? namesFrom = null)
    {
        using var archive = MpqArchive.Open(path, true);
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (namesFrom != null)
            foreach (var n in ListFile(namesFrom)) names.Add(n);
        foreach (var n in KnownNamesList) names.Add(n);
        foreach (var n in ListFileOf(archive)) names.Add(n);
        archive.AddFileNames(names);

        var files = new List<(string Name, uint Size, uint Packed, byte[] Data)>();
        int unnamed = 0;
        for (int i = 0; i < archive.Count; i++)
        {
            var e = archive[i];
            if (e.FileName is null) { if (e.FileSize > 0) unnamed++; continue; }
            byte[] data;
            try { using var s = archive.OpenFile(e); using var ms = new MemoryStream(); s.CopyTo(ms); data = ms.ToArray(); }
            catch { data = Array.Empty<byte>(); }
            files.Add((e.FileName, e.FileSize, e.CompressedSize, data));
        }

        long total = new FileInfo(path).Length;
        Console.WriteLine($"Карта: {Path.GetFileName(path)}  {total / 1048576.0:F1} МБ, файлов {files.Count} (без имени: {unnamed})");
        Console.WriteLine();
        Console.WriteLine("== По категориям (в архиве / распаковано)");
        foreach (var g in files.GroupBy(f => Category(f.Name)).OrderByDescending(g => g.Sum(f => (long)f.Packed)))
        {
            long p = g.Sum(f => (long)f.Packed), u = g.Sum(f => (long)f.Size);
            Console.WriteLine($"  {g.Key,-22} {g.Count(),5} шт  {p / 1048576.0,7:F2} МБ  ({100.0 * p / total,4:F1}%)   распак. {u / 1048576.0,7:F2} МБ");
        }

        Console.WriteLine();
        Console.WriteLine("== Крупнейшие файлы (в архиве)");
        foreach (var f in files.OrderByDescending(f => f.Packed).Take(12))
            Console.WriteLine($"  {f.Packed / 1024.0,9:F0} КБ  {f.Name}");

        var corpus = new StringBuilder();
        foreach (var f in files)
        {
            string ext = Path.GetExtension(f.Name).ToLowerInvariant();
            bool isText = f.Name.StartsWith("war3map", StringComparison.OrdinalIgnoreCase) || ext is ".txt" or ".slk" or ".j" or ".lua" or ".fdf" or ".toc" or ".ai";
            if (isText || ext is ".mdx" or ".mdl")
                corpus.Append(Encoding.Latin1.GetString(f.Data).ToLowerInvariant().Replace('/', '\\')).Append('\n');
        }
        string text = corpus.ToString();

        var assetExt = new HashSet<string> { ".mdx", ".mdl", ".blp", ".tga", ".dds", ".wav", ".mp3", ".flac", ".ogg", ".mid" };
        var assets = files.Where(f => assetExt.Contains(Path.GetExtension(f.Name).ToLowerInvariant())
                                   && !f.Name.StartsWith("war3map", StringComparison.OrdinalIgnoreCase)).ToList();
        var unused = assets.Where(f => !IsReferenced(f.Name, text)).ToList();
        Console.WriteLine();
        Console.WriteLine($"== Похоже не используются: {unused.Count} из {assets.Count} ресурсов, {unused.Sum(f => (long)f.Packed) / 1048576.0:F2} МБ в архиве");
        foreach (var f in unused.OrderByDescending(f => f.Packed).Take(25))
            Console.WriteLine($"  {f.Packed / 1024.0,8:F0} КБ  {f.Name}");
        if (unused.Count > 25) Console.WriteLine($"  ... и ещё {unused.Count - 25}");

        long cur = 0, best = 0;
        foreach (var f in files.Where(f => f.Data.Length > 0))
        {
            long b = 0;
            for (int o = 0; o < f.Data.Length; o += 4096)
            {
                int len = Math.Min(4096, f.Data.Length - o);
                using var ms = new MemoryStream();
                using (var z = new System.IO.Compression.ZLibStream(ms, System.IO.Compression.CompressionLevel.SmallestSize, true)) z.Write(f.Data, o, len);
                b += Math.Min(len, ms.Length + 1);
            }
            cur += f.Packed; best += b;
        }
        Console.WriteLine();
        Console.WriteLine($"== Сжатие: сейчас {cur / 1048576.0:F2} МБ, zlib-9 по секторам {best / 1048576.0:F2} МБ ({100.0 * (cur - best) / cur:F1}% экономии)");

        var blps = files.Where(f => f.Name.EndsWith(".blp", StringComparison.OrdinalIgnoreCase) && f.Data.Length > 28 && Encoding.ASCII.GetString(f.Data, 0, 4) == "BLP1").ToList();
        Console.WriteLine();
        Console.WriteLine($"== Текстуры BLP1: {blps.Count}");
        var byRes = blps.GroupBy(f => { int w = BitConverter.ToInt32(f.Data, 12), h = BitConverter.ToInt32(f.Data, 16); return Math.Max(w, h); })
                        .OrderByDescending(g => g.Key);
        foreach (var g in byRes)
            Console.WriteLine($"  до {g.Key,5}px: {g.Count(),5} шт, {g.Sum(f => (long)f.Packed) / 1048576.0,6:F2} МБ");
        Console.WriteLine("  крупнейшие:");
        foreach (var f in blps.OrderByDescending(f => f.Packed).Take(8))
        {
            int comp = BitConverter.ToInt32(f.Data, 4), w = BitConverter.ToInt32(f.Data, 12), h = BitConverter.ToInt32(f.Data, 16), mips = BitConverter.ToInt32(f.Data, 24);
            Console.WriteLine($"    {f.Packed / 1024.0,6:F0} КБ  {w}x{h} {(comp == 0 ? "JPEG" : "палитра")} mips={mips}  {f.Name}");
        }

        var dups = files.Where(f => f.Data.Length > 0).GroupBy(f => Convert.ToHexString(SHA1.HashData(f.Data))).Where(g => g.Count() > 1).ToList();
        Console.WriteLine();
        Console.WriteLine($"== Дубликаты: {dups.Count} групп, лишних {dups.Sum(g => g.Skip(1).Sum(f => (long)f.Packed)) / 1048576.0:F2} МБ");
        foreach (var g in dups.OrderByDescending(g => g.Skip(1).Sum(f => (long)f.Packed)).Take(10))
            Console.WriteLine($"  {g.First().Packed / 1024.0,8:F0} КБ × {g.Count()}: {string.Join(" | ", g.Select(f => f.Name).Take(3))}");
    }

    public static List<string> ListFile(string mapPath)
    {
        using var a = MpqArchive.Open(mapPath, true);
        return ListFileOf(a);
    }

    public static List<string> ListFileOf(MpqArchive a)
    {
        var res = new List<string>();
        if (a.TryOpenFile("(listfile)", out var ls))
            using (var r = new StreamReader(ls, Encoding.Latin1))
                foreach (var l in r.ReadToEnd().Split(new[] { '\n', '\r', ';' }))
                    if (l.Trim().Length > 0) res.Add(l.Trim());
        res.AddRange(ReadImportList(a));
        return res;
    }

    static List<string> ReadImportList(MpqArchive archive)
    {
        var list = new List<string>();
        if (!archive.TryOpenFile("war3map.imp", out var s)) return list;
        using var r = new BinaryReader(s);
        try
        {
            r.ReadInt32();
            int n = r.ReadInt32();
            for (int i = 0; i < n; i++)
            {
                byte flag = r.ReadByte();
                var sb = new List<byte>();
                byte b;
                while ((b = r.ReadByte()) != 0) sb.Add(b);
                string name = Encoding.Latin1.GetString(sb.ToArray());
                list.Add(flag == 5 || flag == 8 ? name : "war3mapImported\\" + name);
                list.Add(name);
            }
        }
        catch (EndOfStreamException) { }
        return list;
    }

    static bool IsReferenced(string name, string text)
    {
        string n = name.ToLowerInvariant().Replace('/', '\\');
        string noExt = Path.ChangeExtension(n, null)!;
        var keys = new List<string> { n, noExt };
        if (noExt.StartsWith("war3mapimported\\")) keys.Add(noExt.Substring(16));
        if (noExt.EndsWith("_portrait")) keys.Add(noExt[..^9]);
        string file = Path.GetFileName(noExt);
        if (file.StartsWith("disbtn")) keys.Add("btn" + file.Substring(6));
        if (file.StartsWith("dispas")) keys.Add("pas" + file.Substring(6));
        foreach (var k in keys)
        {
            int i = 0;
            while ((i = text.IndexOf(k, i, StringComparison.Ordinal)) >= 0)
            {
                char before = i > 0 ? text[i - 1] : '\0';
                if (!char.IsLetterOrDigit(before) || k.StartsWith("btn") || k.StartsWith("pas")) return true;
                i++;
            }
        }
        return false;
    }

    static string Category(string name)
    {
        string n = name.ToLowerInvariant();
        string ext = Path.GetExtension(n);
        if (n is "war3map.j" or "war3map.lua" or "scripts\\war3map.j") return "скрипт";
        if (n == "war3map.wts") return "строки (wts)";
        if (ext is ".w3u" or ".w3t" or ".w3a" or ".w3b" or ".w3d" or ".w3q" or ".w3h" || ext == ".slk") return "объектные данные";
        if (n is "war3map.w3e" or "war3map.shd" or "war3map.wpm" or "war3map.mmp" or "war3mapmap.blp" or "war3mappath.tga") return "ландшафт/миникарта";
        if (n is "war3map.doo" or "war3mapunits.doo") return "декорации/юниты";
        if (n is "war3map.wtg" or "war3map.wct") return "триггеры редактора";
        if (ext is ".mdx" or ".mdl") return "модели";
        if (ext is ".blp" or ".tga" or ".dds") return "текстуры";
        if (ext is ".wav" or ".mp3" or ".flac" or ".ogg" or ".mid") return "звуки";
        return "прочее";
    }
}
