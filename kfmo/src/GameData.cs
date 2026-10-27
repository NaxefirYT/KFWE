using War3Net.IO.Mpq;

namespace KFMO;

sealed class GameFiles : IDisposable
{
    readonly List<MpqArchive> archives = new();
    public readonly List<string> Skipped = new();

    public GameFiles(string war3Dir)
    {
        foreach (var name in new[] { "War3Patch.mpq", "War3xlocal.mpq", "War3x.mpq", "war3.mpq" })
        {
            var p = Path.Combine(war3Dir, name);
            if (!File.Exists(p)) continue;
            try { archives.Add(MpqArchive.Open(p, false)); }
            catch (Exception ex) { Skipped.Add(name + ": " + ex.Message); }
        }
        if (archives.Count == 0) throw new FileNotFoundException("Не найдены MPQ игры в " + war3Dir + (Skipped.Count > 0 ? " (" + string.Join("; ", Skipped) + ")" : ""));
    }

    public byte[]? Read(string name)
    {
        foreach (var a in archives)
        {
            try
            {
                if (a.TryOpenFile(name, out var s))
                {
                    using (s)
                    using (var ms = new MemoryStream())
                    {
                        s.CopyTo(ms);
                        return ms.ToArray();
                    }
                }
            }
            catch { }
        }
        return null;
    }

    public static string? OverrideDir;

    public static string DefaultDir()
    {
        if (!string.IsNullOrWhiteSpace(OverrideDir) && File.Exists(Path.Combine(OverrideDir, "war3.mpq"))) return OverrideDir;
        foreach (var d in Candidates())
            if (!string.IsNullOrWhiteSpace(d) && File.Exists(Path.Combine(d, "war3.mpq"))) return d.TrimEnd((char)92, '/');
        return @"C:\Games\Warcraft 3";
    }

    static IEnumerable<string> Candidates()
    {
        if (OperatingSystem.IsWindows())
        {
            foreach (var (root, key) in new[] {
                (Microsoft.Win32.Registry.CurrentUser, @"Software\Blizzard Entertainment\Warcraft III"),
                (Microsoft.Win32.Registry.LocalMachine, @"SOFTWARE\WOW6432Node\Blizzard Entertainment\Warcraft III"),
                (Microsoft.Win32.Registry.LocalMachine, @"SOFTWARE\Blizzard Entertainment\Warcraft III") })
            {
                string? v = null;
                try { using var k = root.OpenSubKey(key); v = k?.GetValue("InstallPath") as string; } catch { }
                if (v != null) yield return v;
            }
        }
        yield return @"C:\Games\Warcraft 3";
        yield return @"C:\Program Files (x86)\Warcraft III";
        yield return @"C:\Program Files\Warcraft III";
    }

    public void Dispose() { foreach (var a in archives) a.Dispose(); }
}
