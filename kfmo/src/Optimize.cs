using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
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
        public bool ReforgedSkins = true;
        public bool JassMin;
        public bool KeyReduce;
        public bool IconOneMip;
        public int TexMax;
        public bool NoPreview;
        public bool RequireSlk;
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
            Console.WriteLine(KfLang.F("В карте {0} файлов без имени (нет в listfile). Чтобы их не потерять, укажите --names <исходная карта с listfile>.", unnamed));
            return 2;
        }
        long inSize = Directory.Exists(o.Input) ? files.Sum(f => (long)f.Data.Length) : new FileInfo(o.Input).Length;
        Console.WriteLine(KfLang.F("Вход: {0} {1:F2} МБ, файлов {2}, чтение {3:F1} c", Path.GetFileName(o.Input), inSize / 1048576.0, files.Count, total.Elapsed.TotalSeconds));

        foreach (var (rn, rp) in o.Replace)
        {
            files.RemoveAll(f => f.Name.Equals(rn, StringComparison.OrdinalIgnoreCase));
            files.Add((rn, File.ReadAllBytes(rp)));
            Console.WriteLine(KfLang.F("  подменён {0}", rn));
        }

        if (o.NoPreview && files.RemoveAll(f => f.Name.Equals("war3mapPreview.tga", StringComparison.OrdinalIgnoreCase)) > 0)
            Console.WriteLine(KfLang.T("  превью карты удалено"));

        if (o.StripEditor)
        {
            int before = files.Count;
            files.RemoveAll(f => EditorOnly.Contains(f.Name, StringComparer.OrdinalIgnoreCase));
            var w3i = files.FirstOrDefault(f => f.Name.Equals("war3map.w3i", StringComparison.OrdinalIgnoreCase)).Data;
            bool hasScript = files.Any(f => f.Name.Equals("war3map.j", StringComparison.OrdinalIgnoreCase) || f.Name.Equals(@"scripts\war3map.j", StringComparison.OrdinalIgnoreCase));
            if (hasScript && SlkConv.W3iFlags(w3i) is int fl && (fl & 4) == 0)
                files.RemoveAll(f => f.Name.Equals("war3mapUnits.doo", StringComparison.OrdinalIgnoreCase));
            Console.WriteLine(KfLang.F("  данные только для редактора: удалено {0} файлов", before - files.Count));
        }

        if (o.Slk && !RunSlk(files, o) && o.RequireSlk)
        {
            Console.WriteLine(KfLang.T("SLK не сделан, карта для теста не собрана"));
            return 5;
        }

        var sw0 = Stopwatch.StartNew();
        var sr = Strings.Run(files);
        if (sr != null)
            Console.WriteLine(KfLang.F("  строки: встроено {0} ссылок в данные объектов, wts {1} -> {2} строк ({3} -> {4} КБ), {5:F1} c", sr.Inlined, sr.Before, sr.After, sr.BytesBefore / 1024, sr.BytesAfter / 1024, sw0.Elapsed.TotalSeconds));

        if (o.JassMin) RunJassMin(files);

        var sw = Stopwatch.StartNew();
        int mdxCleaned = 0, mdxBytes = 0;
        Parallel.For(0, files.Count, i =>
        {
            if (!files[i].Name.EndsWith(".mdx", StringComparison.OrdinalIgnoreCase)) return;
            var data = (byte[])files[i].Data.Clone();
            if (MdxClean.Clean(data, out int z)) { files[i] = (files[i].Name, data); Interlocked.Increment(ref mdxCleaned); Interlocked.Add(ref mdxBytes, z); }
        });
        Console.WriteLine(KfLang.F("  модели: зачищено {0} файлов ({1} КБ мусора в строковых полях), {2:F1} c", mdxCleaned, mdxBytes / 1024, sw.Elapsed.TotalSeconds));
        if (o.KeyReduce)
        {
            var ks = new MdxKeys.Stats(); int kf = 0;
            for (int i = 0; i < files.Count; i++)
            {
                if (!files[i].Name.EndsWith(".mdx", StringComparison.OrdinalIgnoreCase)) continue;
                var r = MdxKeys.Apply(files[i].Data, ks);
                if (r != null) { files[i] = (files[i].Name, r); kf++; }
            }
            Console.WriteLine(KfLang.F("  модели: лишние ключи анимации: {0} -> {1} ({2} моделей)", ks.Before, ks.After, kf));
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
            Console.WriteLine(KfLang.F("  модели: округление (нормали {0}, поворот {1}, перемещение {2}, масштаб {3} бит): {4} моделей, нормалей {5}, ключей {6}, не распознано {7}", o.Quant.Normals, o.Quant.Rotation, o.Quant.Translation, o.Quant.Scaling, ok, st.Normals, st.Keys, fail));
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
            string q = o.TexQuality > 0 && psnr.Count > 0 ? KfLang.F(", PSNR среднее {0:F1} дБ, минимум {1:F1}", psnr.Average(), psnr.Min()) : KfLang.T(", без потерь (проверено попиксельно)");
            Console.WriteLine(KfLang.F("  текстуры: {0:F2} -> {1:F2} МБ, изменено {2}, оставлено {3}", texBefore / 1048576.0, texAfter / 1048576.0, texChanged, texSkipped) + q
                + (o.IconOneMip ? KfLang.F(", иконок без мипмапов {0}", icons) : "") + (o.TexMax > 0 ? KfLang.F(", уменьшено до {0}px: {1}", o.TexMax, shrunk) : "")
                + KfLang.F(", {0:F1} с", sw.Elapsed.TotalSeconds));
        }

        sw.Restart();
        var bytes = MpqWriter.Write(files, hm3w, o.Level, o.Listfile, o.SectorShift);
        File.WriteAllBytes(o.Output, bytes);
        Console.WriteLine(KfLang.F("  упаковка (libdeflate {0}): одинаковых файлов хранится один раз: {1}, {2:F1} c", o.Level, MpqWriter.Deduplicated, sw.Elapsed.TotalSeconds));

        sw.Restart();
        int bad = Verify(o.Output, files);
        Console.WriteLine(KfLang.F("  проверка: {0}, {1:F1} c", bad == 0 ? KfLang.T("все файлы читаются и совпадают") : KfLang.F("ОШИБОК: {0}", bad), sw.Elapsed.TotalSeconds));
        if (bad != 0) { File.Delete(o.Output); return 3; }

        long outSize = new FileInfo(o.Output).Length;
        Console.WriteLine(KfLang.F("Выход: {0} {1:F2} МБ ({2:F1}% меньше), всего {3:F1} c", Path.GetFileName(o.Output), outSize / 1048576.0, 100.0 * (inSize - outSize) / inSize, total.Elapsed.TotalSeconds));
        return 0;
    }

    static bool RunSlk(List<(string Name, byte[] Data)> files, Options o)
    {
        var sw = Stopwatch.StartNew();
        byte[]? Get(string n) => files.FirstOrDefault(f => f.Name.Equals(n, StringComparison.OrdinalIgnoreCase)).Data;
        if (files.Any(f => f.Name.EndsWith(".slk", StringComparison.OrdinalIgnoreCase) && f.Name.StartsWith("units", StringComparison.OrdinalIgnoreCase)))
        {
            Console.WriteLine(KfLang.T("  объекты: в карте уже есть SLK-таблицы, конвертация пропущена"));
            return true;
        }
        GameFiles g;
        try { g = new GameFiles(GameFiles.DefaultDir()); }
        catch (Exception ex) { Console.WriteLine(KfLang.T("  объекты: не найдена игра, SLK пропущен (") + ex.Message + ")"); return false; }
        using (g)
        {
            foreach (var s in g.Skipped) Console.WriteLine(KfLang.T("  игра: архив не читается, пропущен: ") + s);
            SlkConv conv;
            try { conv = SlkConv.FromGame(g); }
            catch (Exception ex) { Console.WriteLine(KfLang.T("  объекты: данные игры неполные, SLK пропущен (") + ex.Message + ")"); return false; }
            conv.RemoveUnused = !o.KeepUnused;
            conv.ReforgedSkins = o.ReforgedSkins;
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
            Console.WriteLine(KfLang.F("  объекты -> SLK: правки {0} -> {1} КБ, таблицы+TXT {2} КБ, выкинуто неиспользуемых {3} из {4}, {5:F1} c", before / 1024, after / 1024, slkBytes / 1024, conv.Removed, conv.Total, sw.Elapsed.TotalSeconds));
            if (conv.UnknownParent > 0) Console.WriteLine(KfLang.F("    объектов с неизвестным родителем (оставлены как есть): {0}", conv.UnknownParent));
        }
        return true;
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
        if (common == null || bj == null) { Console.WriteLine(KfLang.T("  скрипт: нет common.j/Blizzard.j игры, сжатие пропущено")); return; }
        var src = Encoding.UTF8.GetString(files[ji].Data);
        var min = JassMin.Minify(src, JassMin.Words(common).Concat(JassMin.Words(bj)), out var r);
        if (min == null) return;
        var check = Pjass(common, bj, min);
        if (check == false) { Console.WriteLine(KfLang.T("  скрипт: сжатый вариант не прошёл pjass, оставлен исходный")); return; }
        var bytes = Encoding.UTF8.GetBytes(min);
        Console.WriteLine(KfLang.F("  скрипт: {0} -> {1} КБ, переименовано функций {2}, глобальных {3}, локальных {4}", files[ji].Data.Length / 1024, bytes.Length / 1024, r.Functions, r.Globals, r.Locals)
            + (check == null ? KfLang.T(" (pjass не найден, без проверки)") : KfLang.T(", pjass: ок")) + KfLang.F(", {0:F1} с", sw.Elapsed.TotalSeconds));
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

    /// <summary>Packs a directory into a .w3x/.w3m map or .w3n campaign using bounded-memory streaming I/O.</summary>
    public static int Pack(string input, string output, int level)
    {
        var sw = Stopwatch.StartNew();
        if (!Directory.Exists(input)) { Console.WriteLine(KfLang.T("нет папки ") + input); return 1; }

        bool isCampaign = output.EndsWith(".w3n", StringComparison.OrdinalIgnoreCase);
        string root = Path.GetFullPath(input);
        string outFull = Path.GetFullPath(output);
        if (Path.GetDirectoryName(outFull) is { Length: > 0 } outDir)
            Directory.CreateDirectory(outDir);
        string tmp = $"{outFull}.kfmo.{Environment.ProcessId}.tmp";

        var entries = new List<MpqWriter.FileEntry>();
        foreach (var f in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
        {
            string full = Path.GetFullPath(f);
            if (full.Equals(outFull, StringComparison.OrdinalIgnoreCase) || full.Equals(tmp, StringComparison.OrdinalIgnoreCase))
                continue;
            var rel = Encoding.Latin1.GetString(Encoding.UTF8.GetBytes(Path.GetRelativePath(root, f).Replace('/', '\\')));
            if (rel.StartsWith("(") && rel.EndsWith(")")) continue;
            bool raw = isCampaign && (rel.EndsWith(".w3x", StringComparison.OrdinalIgnoreCase) || rel.EndsWith(".w3m", StringComparison.OrdinalIgnoreCase));
            entries.Add(new MpqWriter.FileEntry(rel, full, new FileInfo(full).Length, raw));
        }
        entries.Sort((a, b) => StringComparer.Ordinal.Compare(a.Name, b.Name));

        byte[]? GetDisk(string name)
        {
            foreach (var e in entries)
                if (e.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                    return File.ReadAllBytes(e.DiskPath);
            return null;
        }

        var hm3w = isCampaign ? CampaignHeader(GetDisk) : MapHeader(GetDisk);
        if (hm3w == null)
        {
            Console.WriteLine(isCampaign ? KfLang.T("не разобран ") + "war3campaign.w3f" : KfLang.T("не разобран war3map.w3i"));
            return 4;
        }

        try
        {
            using (var fs = new FileStream(tmp, FileMode.Create, FileAccess.ReadWrite, FileShare.None, 65536))
                MpqWriter.WriteStream(entries, fs, hm3w, level, listfile: true, sectorShift: 7);

            int bad = VerifyStream(tmp, entries);
            if (bad != 0)
            {
                Console.WriteLine(KfLang.F("проверка: ОШИБОК {0}", bad));
                return 3;
            }

            long outSize = new FileInfo(tmp).Length;
            long inSize = entries.Sum(f => f.Size);
            File.Move(tmp, outFull, overwrite: true);
            Console.WriteLine(KfLang.F("упаковано {0} файлов, {1:F2} -> {2:F2} МБ, {3:F1} c", entries.Count, inSize / 1048576.0, outSize / 1048576.0, sw.Elapsed.TotalSeconds));
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
            return 2;
        }
        finally
        {
            try { if (File.Exists(tmp)) File.Delete(tmp); } catch { }
        }
    }

    static byte[] BuildHm3wHeader(byte[] name, int flags, int players)
    {
        var hdr = new byte[512];
        int len = Math.Min(name.Length, 512 - 8 - 9);
        while (len > 0 && len < name.Length && (name[len] & 0xC0) == 0x80) len--;
        Encoding.ASCII.GetBytes("HM3W").CopyTo(hdr, 0);
        Array.Copy(name, 0, hdr, 8, len);
        BitConverter.GetBytes(flags).CopyTo(hdr, 8 + len + 1);
        BitConverter.GetBytes(players).CopyTo(hdr, 8 + len + 5);
        return hdr;
    }

    internal static byte[]? MapHeader(List<(string Name, byte[] Data)> files) =>
        MapHeader(n => files.FirstOrDefault(f => f.Name.Equals(n, StringComparison.OrdinalIgnoreCase)).Data);

    internal static byte[]? MapHeader(Func<string, byte[]?> get)
    {
        var w3i = get("war3map.w3i");
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
            if (s.StartsWith("TRIGSTR_") && int.TryParse(s.AsSpan(8), out int id) && get("war3map.wts") is byte[] wts &&
                Strings.ParseWts(wts).TryGetValue(id, out var text))
                name = Encoding.UTF8.GetBytes(text);
            return BuildHm3wHeader(name, flags & 0x3FFFFF, humans);
        }
        catch (EndOfStreamException) { return null; }
    }

    internal static byte[]? CampaignHeader(Func<string, byte[]?> get)
    {
        var w3f = get("war3campaign.w3f");
        if (w3f == null) return null;
        try
        {
            var r = new BinaryReader(new MemoryStream(w3f));
            byte[] Z() { var b = new List<byte>(); for (byte c; (c = r.ReadByte()) != 0;) b.Add(c); return b.ToArray(); }
            r.ReadInt32(); r.ReadInt32(); r.ReadInt32();
            var name = Z();
            var s = Encoding.UTF8.GetString(name);
            if (s.Contains("TRIGSTR_") && get("war3campaign.wts") is byte[] wts)
            {
                var map = Strings.ParseWts(wts);
                s = Regex.Replace(s, @"TRIGSTR_(\d+)", m =>
                    int.TryParse(m.Groups[1].Value, out int id) && map.TryGetValue(id, out var text) ? text : m.Value);
                name = Encoding.UTF8.GetBytes(s);
            }
            return BuildHm3wHeader(name, 0, 1);
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
                if (!ms.ToArray().AsSpan().SequenceEqual(data)) { bad++; Console.WriteLine(KfLang.F("    не совпадает: {0}", name)); }
            }
            catch (Exception ex) { bad++; Console.WriteLine(KfLang.F("    не читается: {0}: {1}", name, ex.Message)); }
        }
        return bad;
    }

    static int VerifyStream(string path, IReadOnlyList<MpqWriter.FileEntry> expected)
    {
        using var a = MpqArchive.Open(path, true);
        a.AddFileNames(expected.Select(f => f.Name));
        var bufA = new byte[65536];
        var bufB = new byte[65536];
        int bad = 0;
        foreach (var f in expected)
        {
            try
            {
                using var s = a.OpenFile(f.Name);
                using var fs = File.OpenRead(f.DiskPath);
                while (true)
                {
                    int ra = s.ReadAtLeast(bufA, bufA.Length, throwOnEndOfStream: false);
                    int rb = fs.ReadAtLeast(bufB, bufB.Length, throwOnEndOfStream: false);
                    if (ra != rb || !bufA.AsSpan(0, ra).SequenceEqual(bufB.AsSpan(0, rb)))
                    {
                        bad++;
                        Console.WriteLine(KfLang.F("    не совпадает: {0}", f.Name));
                        break;
                    }
                    if (ra == 0) break;
                }
            }
            catch (Exception ex) { bad++; Console.WriteLine(KfLang.F("    не читается: {0}: {1}", f.Name, ex.Message)); }
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
