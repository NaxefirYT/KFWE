using BitMiracle.LibJpeg.Classic;

namespace KFMO;

static class JpegTools
{
    public static byte[] Lossless(byte[] jpeg)
    {
        var d = new jpeg_decompress_struct(new jpeg_error_mgr());
        d.jpeg_stdio_src(new MemoryStream(jpeg));
        d.jpeg_read_header(true);
        var coefs = d.jpeg_read_coefficients();
        var outMs = new MemoryStream();
        var c = new jpeg_compress_struct(new jpeg_error_mgr());
        c.jpeg_stdio_dest(outMs);
        d.jpeg_copy_critical_parameters(c);
        c.Optimize_coding = true;
        c.Write_JFIF_header = false;
        c.Write_Adobe_marker = false;
        c.jpeg_write_coefficients(coefs);
        c.jpeg_finish_compress();
        d.jpeg_finish_decompress();
        return outMs.ToArray();
    }

    public static (byte[] Pixels, int W, int H, int Comps) Decode(byte[] jpeg)
    {
        var d = new jpeg_decompress_struct(new jpeg_error_mgr());
        d.jpeg_stdio_src(new MemoryStream(jpeg));
        d.jpeg_read_header(true);
        d.Out_color_space = d.Jpeg_color_space;
        d.jpeg_start_decompress();
        int w = d.Output_width, h = d.Output_height, n = d.Output_components;
        var px = new byte[w * h * n];
        var row = new byte[1][];
        row[0] = new byte[w * n];
        while (d.Output_scanline < h)
        {
            int y = d.Output_scanline;
            d.jpeg_read_scanlines(row, 1);
            Buffer.BlockCopy(row[0], 0, px, y * w * n, w * n);
        }
        d.jpeg_finish_decompress();
        return (px, w, h, n);
    }

    public static byte[] Encode(byte[] px, int w, int h, int comps, J_COLOR_SPACE space, int quality)
    {
        var ms = new MemoryStream();
        var c = new jpeg_compress_struct(new jpeg_error_mgr());
        c.jpeg_stdio_dest(ms);
        c.Image_width = w;
        c.Image_height = h;
        c.Input_components = comps;
        c.In_color_space = space;
        c.jpeg_set_defaults();
        c.jpeg_set_colorspace(space);
        c.jpeg_set_quality(quality, true);
        c.Optimize_coding = true;
        c.Write_JFIF_header = false;
        c.Write_Adobe_marker = false;
        c.jpeg_start_compress(true);
        var row = new byte[1][];
        row[0] = new byte[w * comps];
        while (c.Next_scanline < h)
        {
            Buffer.BlockCopy(px, c.Next_scanline * w * comps, row[0], 0, w * comps);
            c.jpeg_write_scanlines(row, 1);
        }
        c.jpeg_finish_compress();
        return ms.ToArray();
    }

    public static J_COLOR_SPACE ColorSpaceOf(byte[] jpeg)
    {
        var d = new jpeg_decompress_struct(new jpeg_error_mgr());
        d.jpeg_stdio_src(new MemoryStream(jpeg));
        d.jpeg_read_header(true);
        var s = d.Jpeg_color_space;
        d.jpeg_abort_decompress();
        return s;
    }

    static readonly int[] StdLuma =
    {
        16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99,
    };

    public static int EstimateQuality(byte[] jpeg)
    {
        for (int i = 2; i + 4 < jpeg.Length;)
        {
            if (jpeg[i] != 0xFF) { i++; continue; }
            int marker = jpeg[i + 1];
            if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) { i += 2; continue; }
            int len = (jpeg[i + 2] << 8) | jpeg[i + 3];
            if (marker == 0xDB)
            {
                int p = i + 4, pq = jpeg[p] >> 4;
                if (pq != 0) return 100;
                double sum = 0;
                for (int k = 0; k < 64; k++) sum += jpeg[p + 1 + k] * 100.0 / StdLuma[ZigZag[k]];
                double scale = sum / 64;
                return scale <= 100 ? (int)Math.Round((200 - scale) / 2) : (int)Math.Round(5000 / scale);
            }
            if (marker == 0xDA) break;
            i += 2 + len;
        }
        return 100;
    }

    static readonly int[] ZigZag =
    {
        0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21,
        28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61,
        54, 47, 55, 62, 63,
    };

    public static double Psnr(byte[] a, byte[] b, int comps)
    {
        double se = 0;
        long n = 0;
        for (int i = 0; i < a.Length && i < b.Length; i++)
        {
            if (comps == 4 && i % 4 == 3) continue;
            double d = a[i] - b[i];
            se += d * d;
            n++;
        }
        if (se == 0) return 99;
        return 10 * Math.Log10(255.0 * 255.0 / (se / n));
    }
}
