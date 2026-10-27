using System.Security.Cryptography;
using System.Text;
using War3Net.IO.Mpq;

Console.OutputEncoding = Encoding.UTF8;
if (KFMO.Cli.Dispatch(args, out int cliRc)) return cliRc;
if (args.Length >= 2 && args[0] == "unused")
{
    foreach (var n in Analyzer.Unused(args[1])) Console.WriteLine("U " + n);
    return 0;
}
if (args.Length >= 2 && args[0] == "analyze") { Analyzer.Run(args[1], args.Length >= 4 && args[2] == "--names" ? args[3] : null); return 0; }
if (args.Length >= 3 && args[0] == "extract")
{
    using var a = War3Net.IO.Mpq.MpqArchive.Open(args[1], true);
    var names = new HashSet<string>(Analyzer.KnownNamesList, StringComparer.OrdinalIgnoreCase);
    foreach (var n in Analyzer.ListFileOf(a)) names.Add(n);
    a.AddFileNames(names);
    int cnt = 0;
    for (int i = 0; i < a.Count; i++)
    {
        var e = a[i]; if (e.FileName is null) continue;
        var dst = Path.Combine(args[2], e.FileName.Replace((char)92, Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
        using var s = a.OpenFile(e); using var f = File.Create(dst); s.CopyTo(f); cnt++;
    }
    Console.WriteLine($"извлечено {cnt}");
    return 0;
}
if (args.Length >= 3 && args[0] == "pack")
    return KFMO.Optimize.Pack(args[1], args[2], args.Length >= 5 && args[3] == "--level" ? int.Parse(args[4]) : 6);
if (args.Length >= 3 && args[0] == "optimize")
{
    var o = new KFMO.Optimize.Options { Input = args[1], Output = args[2] };
    for (int i = 3; i < args.Length; i++)
        switch (args[i])
        {
            case "--names": o.NamesFrom = args[++i]; break;
            case "--tex-quality": o.TexQuality = int.Parse(args[++i]); break;
            case "--strip-editor": o.StripEditor = true; break;
            case "--no-slk": o.Slk = false; break;
            case "--replace": { var kv = args[++i].Split('=', 2); o.Replace[kv[0]] = kv[1]; break; }
            case "--keep-unused": o.KeepUnused = true; break;
            case "--no-skins": o.ReforgedSkins = false; break;
            case "--game": KFMO.GameFiles.OverrideDir = args[++i]; break;
            case "--no-listfile": o.Listfile = false; break;
            case "--level": o.Level = int.Parse(args[++i]); break;
            case "--mdx-quant": o.Quant = KFMO.MdxQuant.Bits.Release(); break;
            case "--mdx-quant-safe": o.Quant = new KFMO.MdxQuant.Bits(); break;
            case "--release": o.StripEditor = true; o.Quant = KFMO.MdxQuant.Bits.Release(); o.IconOneMip = true; o.KeyReduce = true; o.JassMin = true; o.SectorShift = 9; break;
            case "--keep-jass": o.JassMin = false; break;
            case "--tex-max": o.TexMax = int.Parse(args[++i]); break;
            case "--test": o.RequireSlk = true; o.Textures = false; o.Quant = null; o.JassMin = false; o.KeyReduce = false; o.IconOneMip = false; o.Level = 1; break;
            case "--no-preview": o.NoPreview = true; break;
            case "--sector": o.SectorShift = int.Parse(args[++i]); break;
            case "--keep-keys": o.KeyReduce = false; break;
            case "--icon-mips": o.IconOneMip = false; break;
            case "--mdx-bits":
                var b = args[++i].Split(',').Select(int.Parse).ToArray();
                o.Quant = new KFMO.MdxQuant.Bits { Normals = b[0], Rotation = b[1], Translation = b[2], Scaling = b[3] };
                break;
        }
    return KFMO.Optimize.Run(o);
}
if (args.Length >= 5 && args[0] == "mdxdiff") { KFMO.MdxDiff.Run(args[1], args[2], args[3], args[4]); return 0; }
if (args.Length >= 3 && args[0] == "blpdebug") { KFMO.TexTest.Debug(args[1], args[2]); return 0; }
if (args.Length >= 2 && args[0] == "textest") { KFMO.TexTest.Run(args[1]); return 0; }
Console.WriteLine("usage: kfmo analyze <map.w3x> | kfmo textest <map.w3x>");
return 1;
