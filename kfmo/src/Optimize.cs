using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text;
using War3Net.IO.Mpq;

namespace KFMO;

static class Optimize
{
    public sealed class Options
    {
        public string Input = "", Output = "";
        public string? NamesFrom;
        public int TexQuality;
        public bool StripEditor;
        public bool Slk = true;
        public bool KeepUnused;
        public bool JassMin;
        public bool KeyReduce;
        public bool IconOneMip;
        public int TexMax;
        public bool NoPreview;
        public int SectorShift = 7;
        public bool Textures = true;
        public Dictionary<string, string> Replace = new(StringComparer.OrdinalIgnoreCase);
        public bool Listfile = true;
        public int Level = 12;
        public MdxQuant.Bits? Quant;
    }

    static readonly string[] EditorOnly = { "war3map.wtg", "war3map.wct", "war3map.imp", "war3map.w3c", "war3map.w3r", "war3map.w3s" };

    public static int Run(Options o)
    {
        var total = Stopwatch.StartNew();
        var files = ReadAll(o, out byte[]? hm3w, out int unnamed);
        if (unnamed > 0)
        {
            Console.WriteLine($"В карте {unnamed} файлов без имени (нет в listfile). Чтобы их не потерять, укажите --names <исходная карта с listfile>.");
            return 2;
        }
        long inSize = Directory.Exists(o.Input) ? files.Sum(f => (long)f.Data.Length) : new FileInfo(o.Input).Length;
        Console.WriteLine($"Вход: {Path.GetFileName(o.Input)} {inSize / 1048576.0:F2} МБ, файлов {files.Count}, чтение {total.Elapsed.TotalSeconds:F1} c");

        foreach (var (rn, rp) in o.Replace)
        {
            files.RemoveAll(f => f.Name.Equals(rn, StringComparison.OrdinalIgnoreCase));
            files.Add((rn, File.ReadAllBytes(rp)));
            Console.WriteLine($"  подменён {rn}");
        }

        if (o.NoPreview && files.RemoveAll(f => f.Name.Equals("war3mapPreview.tga", StringComparison.OrdinalIgnoreCase)) > 0)
            Console.WriteLine("  превью карты удалено");

        if (o.StripEditor)
        {
            int before = files.Count;
            files.RemoveAll(f => EditorOnly.Contains(f.Name, StringComparer.OrdinalIgnoreCase));
            var w3i = files.FirstOrDefault(f => f.Name.Equals("war3map.w3i", StringComparison.OrdinalIgnoreCase)).Data;
            bool hasScript = files.Any(f => f.Name.Equals("war3map.j", StringComparison.OrdinalIgnoreCase) || f.Name.Equals(@"scripts\war3map.j", StringComparison.OrdinalIgnoreCase));
            if (hasScript && SlkConv.W3iFlags(w3i) is int fl && (fl & 4) == 0)
                files.RemoveAll(f => f.Name.Equals("war3mapUnits.doo", StringComparison.OrdinalIgnoreCase));
            Console.WriteLine($"  данные только для редактора: удалено {before - files.Count} файлов");
        }

        if (o.Slk) RunSlk(files, o);

        var sw0 = Stopwatch.StartNew();
        var sr = Strings.Run(files);
        if (sr != null)
            Console.WriteLine($"  строки: встроено {sr.Inlined} ссылок в данные объектов, wts {sr.Before} -> {sr.After} строк ({sr.BytesBefore / 1024} -> {sr.BytesAfter / 1024} КБ), {sw0.Elapsed.TotalSeconds:F1} c");

        if (o.JassMin) RunJassMin(files);

        var sw = Stopwatch.StartNew();
        int mdxCleaned = 0, mdxBytes = 0;
        Parallel.For(0, files.Count, i =>
        {
            if (!files[i].Name.EndsWith(".mdx", StringComparison.OrdinalIgnoreCase)) return;
            var data = (byte[])files[i].Data.Clone();
            if (MdxClean.Clean(data, out int z)) { files[i] = (files[i].Name, data); Interlocked.Increment(ref mdxCleaned); Interlocked.Add(ref mdxBytes, z); }
        });
        Console.WriteLine($"  модели: зачищено {mdxCleaned} файлов ({mdxBytes / 1024} КБ мусора в строковых полях), {sw.Elapsed.TotalSeconds:F1} c");
        if (o.KeyReduce)
        {
            var ks = new MdxKeys.Stats(); int kf = 0;
            for (int i = 0; i < files.Count; i++)
            {
                if (!files[i].Name.EndsWith(".mdx", StringComparison.OrdinalIgnoreCase)) continue;
                var r = MdxKeys.Apply(files[i].Data, ks);
                if (r != null) { files[i] = (files[i].Name, r); kf++; }
            }
            Console.WriteLine($"  модели: лишние ключи анимации: {ks.Before} -> {ks.After} ({kf} моделей)");
        }
        if (o.Quant != null)
        {
            var st = new MdxQuant.Stats(); int ok = 0, fail = 0;
            for (int i = 0; i < files.Count; i++)
            {
                if (!files[i].Name.EndsWith(".mdx", StringComparison.OrdinalIgnoreCase)) continue;
                var data = (byte[])files[i].Data.Clone();
                if (MdxQuant.Apply(data, o.Quant, st)) { files[i] = (files[i].Name, data); ok++; } else fail++;
            }
            Console.WriteLine($"  модели: округление (нормали {o.Quant.Normals}, поворот {o.Quant.Rotation}, перемещение {o.Quant.Translation}, масштаб {o.Quant.Scaling} бит): {ok} моделей, нормалей {st.Normals}, ключей {st.Keys}, не распознано {fail}");
        }

        sw.Restart();
        if (o.Textures)
        {
            long texBefore = 0, texAfter = 0; int texChanged = 0, texSkipped = 0, icons = 0;
            var modelTex = o.IconOneMip || o.TexMax > 0 ? ModelTextures(files) : new HashSet<string>();
            int shrunk = 0;
            var psnr = new ConcurrentBag<double>();
            Parallel.For(0, files.Count, i =>
            {
                if (!files[i].Name.EndsWith(".blp", StringComparison.OrdinalIgnoreCase)) return;
                var src = files[i].Data;
                Interlocked.Add(ref texBefore, src.Length);
                bool icon = o.IconOneMip && IsIcon(files[i].Name) && !modelTex.Contains(files[i].Name.Replace('/', '\\'));
                if (o.TexMax > 0 && !icon && modelTex.Contains(files[i].Name.Replace('/', (char)92)) && TexOpt.Shrink(src, o.TexMax) is { } small)
                {
                    src = small; files[i] = (files[i].Name, small); Interlocked.Increment(ref shrunk);
                }
                var res = TexOpt.Process(src, o.TexQuality, out double p, icon);
                if (icon && res != null) Interlocked.Increment(ref icons);
                if (res != null && res.Length < src.Length)
                {
                    files[i] = (files[i].Name, res);
                    Interlocked.Increment(ref texChanged);
                    Interlocked.Add(ref texAfter, res.Length);
                    if (p < 99) psnr.Add(p);
                }
                else { Interlocked.Increment(ref texSkipped); Interlocked.Add(ref texAfter, src.Length); }
            });
            string q = o.TexQuality > 0 && psnr.Count > 0 ? $", PSNR среднее {psnr.Average():F1} дБ, минимум {psnr.Min():F1}" : ", без потерь (проверено попиксельно)";
            Console.WriteLine($"  текстуры: {texBefore / 1048576.0:F2} -> {texAfter / 1048576.0:F2} МБ, изменено {texChanged}, оставлено {texSkipped}{q}{(o.IconOneMip ? $", иконок без мипмапов {icons}" : "")}{(o.TexMax > 0 ? $", уменьшено до {o.TexMax}px: {shrunk}" : "")}, {sw.Elapsed.TotalSeconds:F1} c");
        }

        sw.Restart();
        var bytes = MpqWriter.Write(files, hm3w, o.Level, o.Listfile, o.SectorShift);
        File.WriteAllBytes(o.Output, bytes);
        Console.WriteLine($"  упаковка (libdeflate {o.Level}): одинаковых файлов хранится один раз: {MpqWriter.Deduplicated}, {sw.Elapsed.TotalSeconds:F1} c");

        sw.Restart();
        int bad = Verify(o.Output, files);
        Console.WriteLine($"  проверка: {(bad == 0 ? "все файлы читаются и совпадают" : $"ОШИБОК: {bad}")}, {sw.Elapsed.TotalSeconds:F1} c");
        if (bad != 0) { File.Delete(o.Output); return 3; }

        long outSize = new FileInfo(o.Output).Length;
        Console.WriteLine($"Выход: {Path.GetFileName(o.Output)} {outSize / 1048576.0:F2} МБ ({100.0 * (inSize - outSize) / inSize:F1}% меньше), всего {total.Elapsed.TotalSeconds:F1} c");
        return 0;
    }

    static void RunSlk(List<(string Name, byte[] Data)> files, Options o)
    {
        var sw = Stopwatch.StartNew();
        byte[]? Get(string n) => files.FirstOrDefault(f => f.Name.Equals(n, StringComparison.OrdinalIgnoreCase)).Data;
        if (files.Any(f => f.Name.EndsWith(".slk", StringComparison.OrdinalIgnoreCase) && f.Name.StartsWith("units", StringComparison.OrdinalIgnoreCase)))
        {
            Console.WriteLine("  объекты: в карте уже есть SLK-таблицы, конвертация пропущена");
            return;
        }
        GameFiles g;
        try { g = new GameFiles(GameFiles.DefaultDir()); }
        catch (Exception ex) { Console.WriteLine("  объекты: не найдена игра, SLK пропущен (" + ex.Message + ")"); return; }
        using (g)
        {
            var conv = SlkConv.FromGame(g);
            conv.RemoveUnused = !o.KeepUnused;
            var wtsBytes = Get("war3map.wts");
            var wts = wtsBytes != null ? Strings.ParseWts(wtsBytes) : new Dictionary<int, string>();
            int next = wts.Count == 0 ? 0 : wts.Keys.Max() + 1;
            var added = new List<(int, string)>();
            var r = conv.Convert(Get, wts, s => { added.Add((next, s)); return "TRIGSTR_" + (next++).ToString("000"); });
            long before = 0, after = 0;
            foreach (var (b, a) in r.ObjBytes.Values) { before += b; after += a; }
            files.RemoveAll(f => r.Remove.Contains(f.Name) || r.Write.ContainsKey(f.Name));
            foreach (var (n, d) in r.Write) files.Add((n, d));
            if (added.Count > 0)
            {
                int wi = files.FindIndex(f => f.Name.Equals("war3map.wts", StringComparison.OrdinalIgnoreCase));
                var nw = Strings.AppendWts(wtsBytes, added);
                if (wi >= 0) files[wi] = (files[wi].Name, nw); else files.Add(("war3map.wts", nw));
            }
            long slkBytes = r.Write.Where(p => !p.Key.StartsWith("war3map", StringComparison.OrdinalIgnoreCase)).Sum(p => (long)p.Value.Length);
            Console.WriteLine($"  объекты -> SLK: правки {before / 1024} -> {after / 1024} КБ, таблицы+TXT {slkBytes / 1024} КБ, выкинуто неиспользуемых {conv.Removed} из {conv.Total}, {sw.Elapsed.TotalSeconds:F1} c");
            if (conv.UnknownParent > 0) Console.WriteLine($"    объектов с неизвестным родителем (оставлены как есть): {conv.UnknownParent}");
        }
    }

    static void RunJassMin(List<(string Name, byte[] Data)> files)
    {
        var sw = Stopwatch.StartNew();
        int ji = files.FindIndex(f => f.Name.Equals("war3map.j", StringComparison.OrdinalIgnoreCase) || f.Name.Equals("scripts" + (char)92 + "war3map.j", StringComparison.OrdinalIgnoreCase));
        if (ji < 0) return;
        string? common = null, bj = null;
        try
        {
            using var g = new GameFiles(GameFiles.DefaultDir());
            var c = g.Read("Scripts" + (char)92 + "common.j"); var b = g.Read("Scripts" + (char)92 + "Blizzard.j");
            if (c != null && b != null) { common = Encoding.UTF8.GetString(c); bj = Encoding.UTF8.GetString(b); }
        }
        catch { }
        if (common == null || bj == null) { Console.WriteLine("  скрипт: нет common.j/Blizzard.j игры, сжатие пропущено"); return; }
        var src = Encoding.UTF8.GetString(files[ji].Data);
        var min = JassMin.Minify(src, JassMin.Words(common).Concat(JassMin.Words(bj)), out var r);
        if (min == null) return;
        var check = Pjass(common, bj, min);
        if (check == false) { Console.WriteLine("  скрипт: сжатый вариант не прошёл pjass, оставлен исходный"); return; }
        var bytes = Encoding.UTF8.GetBytes(min);
        Console.WriteLine($"  скрипт: {files[ji].Data.Length / 1024} -> {bytes.Length / 1024} КБ, переименовано функций {r.Functions}, глобальных {r.Globals}, локальных {r.Locals}{(check == null ? " (pjass не найден, без проверки)" : ", pjass: ок")}, {sw.Elapsed.TotalSeconds:F1} c");
        files[ji] = (files[ji].Name, bytes);
    }

    static bool? Pjass(string common, string bj, string script)
    {
        var exe = new[] { Path.Combine(AppContext.BaseDirectory, "pjass.exe"), Path.Combine(GameFiles.DefaultDir(), "KFWE", "compiler", "pjass", "pjass-latest.exe") }.FirstOrDefault(File.Exists);
        if (exe == null) return null;
        var dir = Path.Combine(Path.GetTempPath(), "kfmo_pjass_" + Environment.ProcessId);
        Directory.CreateDirectory(dir);
        try
        {
            File.WriteAllText(Path.Combine(dir, "common.j"), common);
            File.WriteAllText(Path.Combine(dir, "Blizzard.j"), bj);
            File.WriteAllText(Path.Combine(dir, "war3map.j"), script);
            var psi = new ProcessStartInfo(exe, "common.j Blizzard.j war3map.j") { WorkingDirectory = dir, RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true };
            using var pr = Process.Start(psi)!;
            var output = pr.StandardOutput.ReadToEnd() + pr.StandardError.ReadToEnd();
            pr.WaitForExit();
            bool ok = pr.ExitCode == 0 && output.Contains("Parse successful") && !output.Contains("failed", StringComparison.OrdinalIgnoreCase);
            if (!ok) File.WriteAllText(Path.Combine(Path.GetTempPath(), "kfmo_pjass_last.txt"), output);
            return ok;
        }
        finally { try { Directory.Delete(dir, true); } catch { } }
    }

    static bool IsIcon(string name)
    {
        var f = Path.GetFileName(name.Replace((char)92, '/'));
        foreach (var p in new[] { "BTN", "DISBTN", "PASBTN", "DISPASBTN", "ATC", "DISATC" })
            if (f.StartsWith(p, StringComparison.OrdinalIgnoreCase)) return true;
        return name.Contains("CommandButtons", StringComparison.OrdinalIgnoreCase) || name.Contains("PassiveButtons", StringComparison.OrdinalIgnoreCase);
    }

    static HashSet<string> ModelTextures(List<(string Name, byte[] Data)> files)
    {
        var res = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var (name, d) in files)
        {
            if (!name.EndsWith(".mdx", StringComparison.OrdinalIgnoreCase) || d.Length < 8) continue;
            for (int i = 4; i + 8 <= d.Length;)
            {
                string tag = Encoding.ASCII.GetString(d, i, 4);
                int size = BitConverter.ToInt32(d, i + 4);
                if (size < 0 || i + 8L + size > d.Length) break;
                if (tag == "TEXS")
                    for (int t = i + 8; t + 268 <= i + 8 + size; t += 268)
                    {
                        int z = Array.IndexOf(d, (byte)0, t + 4, 260);
                        var path = Encoding.Latin1.GetString(d, t + 4, (z < 0 ? t + 264 : z) - t - 4);
                        if (path.Length > 0) res.Add(path.Replace('/', '\\'));
                    }
                i += 8 + size;
            }
        }
        return res;
    }

    public static int Pack(string input, string output, int level)
    {
        var sw = Stopwatch.StartNew();
        if (!Directory.Exists(input)) { Console.WriteLine("нет папки " + input); return 1; }
        var files = ReadAll(new Options { Input = input }, out _, out _);
        var hm3w = MapHeader(files);
        if (hm3w == null) { Console.WriteLine("не разобран war3map.w3i"); return 4; }
        var bytes = MpqWriter.Write(files, hm3w, level, true, 7, dedup: false);
        File.WriteAllBytes(output, bytes);
        int bad = Verify(output, files);
        if (bad != 0) { File.Delete(output); Console.WriteLine($"проверка: ОШИБОК {bad}"); return 3; }
        Console.WriteLine($"упаковано {files.Count} файлов, {files.Sum(f => (long)f.Data.Length) / 1048576.0:F2} -> {bytes.Length / 1048576.0:F2} МБ, {sw.Elapsed.TotalSeconds:F1} c");
        return 0;
    }

    internal static byte[]? MapHeader(List<(string Name, byte[] Data)> files)
    {
        byte[]? Get(string n) => files.FirstOrDefault(f => f.Name.Equals(n, StringComparison.OrdinalIgnoreCase)).Data;
        var w3i = Get("war3map.w3i");
        if (w3i == null) return null;
        try
        {
            var r = new BinaryReader(new MemoryStream(w3i));
            byte[] Z() { var b = new List<byte>(); for (byte c; (c = r.ReadByte()) != 0;) b.Add(c); return b.ToArray(); }
            int ver = r.ReadInt32();
            if (ver != 25 && ver != 18) return null;
            r.ReadInt32(); r.ReadInt32();
            var name = Z(); Z(); Z(); Z();
            r.ReadBytes(32 + 16 + 8);
            int flags = r.ReadInt32();
            r.ReadByte();
            if (ver == 25)
            {
                r.ReadInt32(); Z(); Z(); Z(); Z();
                r.ReadInt32(); Z(); Z(); Z(); Z();
                r.ReadInt32(); r.ReadBytes(12 + 4); r.ReadInt32(); Z(); r.ReadByte(); r.ReadBytes(4);
            }
            else
            {
                r.ReadInt32(); Z(); Z(); Z(); r.ReadInt32(); Z(); Z(); Z();
            }
            int np = r.ReadInt32(), humans = 0;
            for (int i = 0; i < np; i++)
            {
                r.ReadInt32();
                if (r.ReadInt32() == 1) humans++;
                r.ReadInt32(); r.ReadInt32(); Z(); r.ReadBytes(8 + 8);
            }
            var s = Encoding.UTF8.GetString(name);
            if (s.StartsWith("TRIGSTR_") && int.TryParse(s.AsSpan(8), out int id) && Get("war3map.wts") is byte[] wts &&
                Strings.ParseWts(wts).TryGetValue(id, out var text))
                name = Encoding.UTF8.GetBytes(text);
            var hdr = new byte[512];
            int len = Math.Min(name.Length, 512 - 8 - 9);
            Encoding.ASCII.GetBytes("HM3W").CopyTo(hdr, 0);
            Array.Copy(name, 0, hdr, 8, len);
            BitConverter.GetBytes(flags & 0x3FFFFF).CopyTo(hdr, 8 + len + 1);
            BitConverter.GetBytes(humans).CopyTo(hdr, 8 + len + 5);
            return hdr;
        }
        catch (EndOfStreamException) { return null; }
    }

    static List<(string Name, byte[] Data)> ReadAll(Options o, out byte[]? hm3w, out int unnamed)
    {
        if (Directory.Exists(o.Input))
        {
            unnamed = 0;
            var list = new List<(string, byte[])>();
            var root = Path.GetFullPath(o.Input);
            foreach (var f in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
            {
                var rel = Encoding.Latin1.GetString(Encoding.UTF8.GetBytes(Path.GetRelativePath(root, f).Replace('/', (char)92)));
                if (rel.StartsWith("(") && rel.EndsWith(")")) continue;
                list.Add((rel, File.ReadAllBytes(f)));
            }
            hm3w = new byte[512];
            Encoding.ASCII.GetBytes("HM3W").CopyTo(hm3w, 0);
            return list;
        }
        var raw = File.ReadAllBytes(o.Input);
        hm3w = raw.Length >= 512 && Encoding.ASCII.GetString(raw, 0, 4) == "HM3W" ? raw.AsSpan(0, 512).ToArray() : null;
        using var archive = MpqArchive.Open(new MemoryStream(raw), true);
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var n in Analyzer.KnownNamesList) names.Add(n);
        foreach (var n in Analyzer.ListFileOf(archive)) names.Add(n);
        if (o.NamesFrom != null)
            foreach (var n in Analyzer.ListFile(o.NamesFrom)) names.Add(n);
        archive.AddFileNames(names);
        var res = new List<(string, byte[])>();
        unnamed = 0;
        for (int i = 0; i < archive.Count; i++)
        {
            var e = archive[i];
            if (e.FileName is null) { unnamed++; continue; }
            if (e.FileName.Equals("(listfile)", StringComparison.OrdinalIgnoreCase) || e.FileName.Equals("(attributes)", StringComparison.OrdinalIgnoreCase)) continue;
            using var s = archive.OpenFile(e);
            using var ms = new MemoryStream();
            s.CopyTo(ms);
            res.Add((e.FileName, ms.ToArray()));
        }
        return res;
    }

    static int Verify(string path, List<(string Name, byte[] Data)> expected)
    {
        using var a = MpqArchive.Open(path, true);
        a.AddFileNames(expected.Select(f => f.Name));
        int bad = 0;
        foreach (var (name, data) in expected)
        {
            try
            {
                using var s = a.OpenFile(name);
                using var ms = new MemoryStream();
                s.CopyTo(ms);
                if (!ms.ToArray().AsSpan().SequenceEqual(data)) { bad++; Console.WriteLine($"    не совпадает: {name}"); }
            }
            catch (Exception ex) { bad++; Console.WriteLine($"    не читается: {name}: {ex.Message}"); }
        }
        return bad;
    }
}

static class TexOpt
{
    public static byte[]? Shrink(byte[] src, int maxSize)
    {
        try
        {
            var blp = Blp1.Parse(src);
            if (blp is not { IsJpeg: true }) return null;
            int drop = 0, w = blp.Width, h = blp.Height;
            while (Math.Max(w, h) > maxSize && drop + 1 < blp.Mips.Count) { drop++; w = Math.Max(1, w / 2); h = Math.Max(1, h / 2); }
            if (drop == 0 || Math.Max(w, h) > maxSize) return null;
            var jpegs = new List<byte[]>();
            for (int m = drop; m < blp.Mips.Count; m++) jpegs.Add(blp.MipJpeg(m));
            blp.Width = w; blp.Height = h;
            var res = blp.BuildFromJpegs(jpegs);
            var chk = Blp1.Parse(res);
            return chk != null && chk.Width == w && chk.Mips.Count == jpegs.Count ? res : null;
        }
        catch { return null; }
    }

    public static byte[]? Process(byte[] src, int quality, out double psnr, bool oneMip = false)
    {
        psnr = 99;
        try
        {
            var blp = Blp1.Parse(src);
            if (blp is not { IsJpeg: true }) return null;
            if (oneMip && blp.Mips.Count > 1)
            {
                blp.Mips.RemoveRange(1, blp.Mips.Count - 1);
                blp.HasMipmaps = 0;
                var r1 = Process(blp.BuildFromJpegs(new[] { blp.MipJpeg(0) }), quality, out psnr) ;
                var plain = blp.BuildFromJpegs(new[] { blp.MipJpeg(0) });
                var best = r1 != null && r1.Length < plain.Length ? r1 : plain;
                var chk = Blp1.Parse(best);
                return chk != null && chk.Mips.Count == 1 ? best : null;
            }
            int origQ = JpegTools.EstimateQuality(blp.MipJpeg(0));
            bool lossy = quality > 0 && origQ > quality;
            var coefs = new List<JpegJoint.Coefs>();
            var origPx = new List<byte[]>();
            for (int m = 0; m < blp.Mips.Count; m++)
            {
                var mip = blp.MipJpeg(m);
                var (px, w, h, n) = JpegTools.Decode(mip);
                origPx.Add(px);
                var basis = lossy ? JpegTools.Encode(px, w, h, n, JpegTools.ColorSpaceOf(mip), quality) : mip;
                coefs.Add(JpegJoint.Read(basis) ?? throw new NotSupportedException());
            }
            var jpegs = JpegJoint.Encode(coefs);
            for (int m = 0; m < jpegs.Count; m++)
            {
                var dec = JpegTools.Decode(jpegs[m]).Pixels;
                if (!lossy && !dec.AsSpan().SequenceEqual(origPx[m])) return null;
                if (lossy && m == 0) psnr = JpegTools.Psnr(origPx[0], dec, 4);
            }
            var res = blp.BuildFromJpegs(jpegs);
            var check = Blp1.Parse(res);
            if (check == null || check.Mips.Count != blp.Mips.Count) return null;
            return res;
        }
        catch { return null; }
    }
}
