namespace KFMO;

static class Cli
{
    public static bool Dispatch(string[] args, out int rc)
    {
        rc = 0;
        if (args.Length >= 2 && args[0] == "gamedump")
        {
            GameDump(args[1]);
            return true;
        }
        if (args.Length >= 3 && args[0] == "meta")
        {
            using var g = new GameFiles(GameFiles.DefaultDir());
            var t = SlkTable.Parse(g.Read(args[1])!);
            Console.WriteLine(string.Join(" | ", t.Columns.Take(26)));
            foreach (var id in t.RowOrder)
            {
                var r = t.Rows[id];
                string line = id + " | " + string.Join(" | ", new[] { "field", "slk", "index", "repeat", "data", "type", "useSpecific", "notSpecific", "useHero", "useUnit", "useItem" }.Select(c => c + "=" + r.GetValueOrDefault(c, "")));
                if (args[2] == "*" || line.Contains(args[2], StringComparison.OrdinalIgnoreCase)) Console.WriteLine(line);
            }
            return true;
        }
        if (args.Length >= 3 && args[0] == "slkdir")
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            using var g = new GameFiles(GameFiles.DefaultDir());
            var conv = SlkConv.FromGame(g);
            Console.WriteLine($"база игры: {sw.Elapsed.TotalSeconds:F1} c");
            string Path1(string n) => Path.Combine(args[1], n.Replace('\\', Path.DirectorySeparatorChar));
            byte[]? Get(string n) => File.Exists(Path1(n)) ? File.ReadAllBytes(Path1(n)) : null;
            var wtsBytes = Get("war3map.wts");
            var wts = wtsBytes != null ? Strings.ParseWts(wtsBytes) : new Dictionary<int, string>();
            int next = wts.Count == 0 ? 0 : wts.Keys.Max() + 1;
            var added = new List<(int, string)>();
            var r = conv.Convert(Get, wts, s => { added.Add((next, s)); return "TRIGSTR_" + (next++).ToString("000"); });
            Console.WriteLine($"конвертация: {sw.Elapsed.TotalSeconds:F1} c; полей выброшено {conv.DroppedFields}, без родителя {conv.UnknownParent}, оставлено объектами {conv.KeptAsObj}, уровни>4 {conv.OverLevel}, строки с , и \" {conv.TxtEscape}, выкинуто неиспользуемых {conv.Removed} из {conv.Total}, не найдено ссылок {conv.NotFound.Distinct().Count()}");
            foreach (var (t, (b, a)) in r.ObjBytes) Console.WriteLine($"  {SlkConv.ObjFile[t]}: {b} -> {a}");
            Directory.CreateDirectory(args[2]);
            foreach (var (n, d) in r.Write)
            {
                var dst = Path.Combine(args[2], n.Replace('\\', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
                File.WriteAllBytes(dst, d);
            }
            if (added.Count > 0) File.WriteAllBytes(Path.Combine(args[2], "war3map.wts"), Strings.AppendWts(wtsBytes, added));
            Console.WriteLine($"записано {r.Write.Count}, удалить {string.Join(",", r.Remove)}, новых строк wts {added.Count}");
            return true;
        }
        if (args.Length >= 2 && args[0] == "mdxbench")
        {
            var mdx = Directory.GetFiles(args[1], "*.mdx", SearchOption.AllDirectories).Select(File.ReadAllBytes).ToList();
            foreach (var d in mdx) MdxClean.Clean(d, out _);
            var presets = new (string Name, MdxQuant.Bits? B)[]
            {
                ("без округления", null),
                ("KFMO сейчас (нормали 8, узлы 10-11)", new MdxQuant.Bits()),
                ("w3x2lni mdx_squf (всё 7 бит, обрезка)", new MdxQuant.Bits { Normals = 7, Rotation = 7, Translation = 7, Scaling = 7, Vertices = 7, Uv = 7, Extents = 7, Truncate = true }),
                ("всё 7 бит, округление", new MdxQuant.Bits { Normals = 7, Rotation = 7, Translation = 7, Scaling = 7, Vertices = 7, Uv = 7, Extents = 7 }),
                ("KFMO + вершины 7, UV 7, рамки 7", new MdxQuant.Bits { Vertices = 7, Uv = 7, Extents = 7 }),
                ("KFMO + вершины 8, UV 9, рамки 8", new MdxQuant.Bits { Vertices = 8, Uv = 9, Extents = 8 }),
                ("KFMO + вершины 10, UV 11, рамки 10", new MdxQuant.Bits { Vertices = 10, Uv = 11, Extents = 10 }),
                ("нормали 6, узлы 7, вершины 7, UV 8, рамки 7", new MdxQuant.Bits { Normals = 6, Rotation = 7, Translation = 7, Scaling = 7, Vertices = 7, Uv = 8, Extents = 7 }),
            };
            using var comp = new LibDeflate.ZlibCompressor(12);
            foreach (var (name, b) in presets)
            {
                long total = 0; int fail = 0;
                foreach (var d0 in mdx)
                {
                    var d = (byte[])d0.Clone();
                    if (b != null && !MdxQuant.Apply(d, b, new MdxQuant.Stats())) fail++;
                    total += MpqWriter.PackFile(d, 65536, comp).Length;
                }
                Console.WriteLine($"{total / 1048576.0,7:F2} МБ  {name}{(fail > 0 ? $"  (не разобрано {fail})" : "")}");
            }
            return true;
        }
        if (args.Length >= 2 && args[0] == "gameread")
        {
            using var g = new GameFiles(GameFiles.DefaultDir());
            var d = g.Read(args[1]);
            Console.WriteLine(d == null ? "нет" : $"{d.Length} байт");
            if (d != null && args.Length >= 3) File.WriteAllBytes(args[2], d);
            return true;
        }
        return false;
    }

    static void GameDump(string outDir)
    {
        using var g = new GameFiles(GameFiles.DefaultDir());
        var list = StdNames.Extra.Concat(new[]
        {
            @"Units\UnitMetaData.slk", @"Units\AbilityMetaData.slk", @"Units\AbilityBuffMetaData.slk",
            @"Units\UpgradeMetaData.slk", @"Units\DestructableMetaData.slk", @"Doodads\DoodadMetaData.slk",
            @"Units\MiscMetaData.slk", @"UI\UnitEditorData.txt", @"Units\UnitGlobalStrings.txt",
        });
        int n = 0;
        foreach (var f in list.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            var d = g.Read(f);
            if (d == null) { Console.WriteLine("нет: " + f); continue; }
            var dst = Path.Combine(outDir, f.Replace('\\', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
            File.WriteAllBytes(dst, d);
            n++;
        }
        Console.WriteLine($"выгружено {n}");
    }
}
