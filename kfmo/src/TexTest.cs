using System.Collections.Concurrent;
using System.Diagnostics;
using War3Net.IO.Mpq;

namespace KFMO;

static class TexTest
{
    public static void Run(string path)
    {
        var blps = new List<(string Name, byte[] Data)>();
        using (var archive = MpqArchive.Open(path, true))
        {
            for (int i = 0; i < archive.Count; i++)
            {
                var e = archive[i];
                if (e.FileName is null || !e.FileName.EndsWith(".blp", StringComparison.OrdinalIgnoreCase)) continue;
                using var s = archive.OpenFile(e); using var ms = new MemoryStream(); s.CopyTo(ms);
                blps.Add((e.FileName, ms.ToArray()));
            }
        }
        var parsed = blps.Select(b => (b.Name, b.Data, Blp: Blp1.Parse(b.Data))).Where(x => x.Blp is { IsJpeg: true }).ToList();
        Console.WriteLine($"BLP всего {blps.Count}, JPEG {parsed.Count}, исходно {parsed.Sum(x => (long)x.Data.Length) / 1048576.0:F2} МБ");

        var qhist = parsed.GroupBy(x => JpegTools.EstimateQuality(x.Blp!.MipJpeg(0)) / 5 * 5).OrderByDescending(g => g.Key);
        Console.WriteLine("Исходное качество JPEG (оценка): " + string.Join("  ", qhist.Select(g => $"q{g.Key}+:{g.Count()}")));

        foreach (int q in new[] { 0, 95, 90, 85, 80 })
        {
            var sw = Stopwatch.StartNew();
            long total = 0; int failed = 0, kept = 0, mismatch = 0;
            var psnrs = new ConcurrentBag<double>();
            Parallel.ForEach(parsed, x =>
            {
                try
                {
                    var blp = x.Blp!;
                    int origQ = JpegTools.EstimateQuality(blp.MipJpeg(0));
                    var coefs = new List<JpegJoint.Coefs>();
                    var origPx = new List<byte[]>();
                    for (int m = 0; m < blp.Mips.Count; m++)
                    {
                        var src = blp.MipJpeg(m);
                        var (px, w, h, n) = JpegTools.Decode(src);
                        origPx.Add(px);
                        byte[] basis = src;
                        if (q != 0 && origQ > q)
                            basis = JpegTools.Encode(px, w, h, n, JpegTools.ColorSpaceOf(src), q);
                        var c = JpegJoint.Read(basis) ?? throw new NotSupportedException();
                        coefs.Add(c);
                    }
                    var jpegs = JpegJoint.Encode(coefs);
                    for (int m = 0; m < jpegs.Count; m++)
                    {
                        var dec = JpegTools.Decode(jpegs[m]).Pixels;
                        if (q == 0 || origQ <= q)
                        {
                            if (!dec.AsSpan().SequenceEqual(origPx[m])) { Interlocked.Increment(ref mismatch); throw new InvalidDataException(); }
                        }
                        else if (m == 0) psnrs.Add(JpegTools.Psnr(origPx[0], dec, 4));
                    }
                    var nb = blp.BuildFromJpegs(jpegs);
                    if (nb.Length < x.Data.Length) Interlocked.Add(ref total, nb.Length);
                    else { Interlocked.Add(ref total, x.Data.Length); Interlocked.Increment(ref kept); }
                }
                catch { Interlocked.Add(ref total, x.Data.Length); Interlocked.Increment(ref failed); }
            });
            var p = psnrs.OrderBy(v => v).ToList();
            string ps = p.Count > 0 ? $"PSNR среднее {p.Average():F1} дБ, худшие 5%: {p[p.Count / 20]:F1} дБ, минимум {p[0]:F1}" : "без потерь";
            Console.WriteLine($"{(q == 0 ? "без потерь" : "качество " + q),-12} {total / 1048576.0,6:F2} МБ ({100.0 * total / parsed.Sum(x => (long)x.Data.Length),5:F1}%)  оставлено как было {kept}, не поддерживается/ошибок {failed} (несовпадений пикселей {mismatch}), {sw.Elapsed.TotalSeconds:F1} c  | {ps}");
        }
    }

    public static void Debug(string path, string name)
    {
        using var archive = MpqArchive.Open(path, true);
        archive.AddFileName(name);
        using var st = archive.OpenFile(name); using var ms = new MemoryStream(); st.CopyTo(ms);
        var data = ms.ToArray();
        var blp = Blp1.Parse(data)!;
        Console.WriteLine($"{name}: {data.Length} байт, {blp.Width}x{blp.Height}, alpha={blp.AlphaBits}, mips={blp.Mips.Count}, header={blp.JpegHeader.Length}");
        var coefs = Enumerable.Range(0, blp.Mips.Count).Select(m => JpegJoint.Read(blp.MipJpeg(m))!).ToList();
        var jpegs = JpegJoint.Encode(coefs);
        var nb = blp.BuildFromJpegs(jpegs);
        var nblp = Blp1.Parse(nb)!;
        Console.WriteLine($"новый: {nb.Length} байт, header={nblp.JpegHeader.Length}");
        for (int m = 0; m < blp.Mips.Count; m++)
            Console.WriteLine($"  mip{m}: было {blp.Mips[m].Length,6}  стало {nblp.Mips[m].Length,6}   ({coefs[m].Width}x{coefs[m].Height})");
        Console.WriteLine("исходный mip1 начало: " + Convert.ToHexString(blp.Mips[1].AsSpan(0, Math.Min(40, blp.Mips[1].Length))));
        Console.WriteLine("новый    mip1 начало: " + Convert.ToHexString(nblp.Mips[1].AsSpan(0, Math.Min(40, nblp.Mips[1].Length))));
        Console.WriteLine("исходный конец mip1: " + Convert.ToHexString(blp.Mips[1].AsSpan(blp.Mips[1].Length - 8)));
    }
}
