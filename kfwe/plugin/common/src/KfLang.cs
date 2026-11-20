using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

static class KfLang
{
    static Dictionary<string, string> dict = new Dictionary<string, string>();

    static bool ready;

    public static string Code = "ru";

    static void Init()
    {
        ready = true;
        try
        {
            var loc = typeof(KfLang).Assembly.Location;
            if (!(loc.Length > 0 && LoadFrom(Path.GetDirectoryName(loc))))
                LoadFrom(AppDomain.CurrentDomain.BaseDirectory);
        }
        catch { }
    }

    public static string T(string s)
    {
        if (!ready) Init();
        string v;
        return s != null && dict.TryGetValue(s, out v) ? v : s;
    }

    public static string F(string s, params object[] args)
    {
        return string.Format(T(s), args);
    }

    public static Dictionary<string, string> Parse(string text)
    {
        var d = new Dictionary<string, string>();
        foreach (var raw in text.Split('\n'))
        {
            var line = raw.Trim();
            if (line.StartsWith("#")) continue;
            int eq = line.IndexOf(" = ", StringComparison.Ordinal);
            if (eq <= 0) continue;
            string k = Unescape(line.Substring(0, eq).TrimEnd()), v = Unescape(line.Substring(eq + 3).TrimStart());
            if (k.Length > 0 && v.Length > 0) d[k] = v;
        }
        return d;
    }

    static string Unescape(string s)
    {
        if (s.IndexOf('\\') < 0) return s;
        var b = new StringBuilder(s.Length);
        for (int i = 0; i < s.Length; i++)
        {
            char c = s[i];
            if (c == '\\' && i + 1 < s.Length && (s[i + 1] == 'n' || s[i + 1] == 't' || s[i + 1] == 's' || s[i + 1] == '\\'))
            {
                c = s[++i];
                b.Append(c == 'n' ? '\n' : c == 't' ? '\t' : c == 's' ? ' ' : '\\');
            }
            else b.Append(c);
        }
        return b.ToString();
    }

    public static void Use(string code, string text)
    {
        ready = true;
        Code = string.IsNullOrEmpty(code) ? "ru" : code;
        dict = Code == "ru" || text == null ? new Dictionary<string, string>() : Parse(text);
    }

    public static string FindRoot(string start)
    {
        for (var d = start; !string.IsNullOrEmpty(d); d = Path.GetDirectoryName(d))
            if (Directory.Exists(Path.Combine(d, Path.Combine("share", "locale")))) return d;
        return null;
    }

    public static string ReadCode(string root)
    {
        try
        {
            var p = Path.Combine(root, Path.Combine("share", Path.Combine("locale", "default")));
            if (!File.Exists(p)) return "ru";
            var c = File.ReadAllText(p).Trim();
            return c.Length > 0 ? c : "ru";
        }
        catch { return "ru"; }
    }

    public static bool LoadFrom(string start)
    {
        var root = FindRoot(start);
        if (root == null) return false;
        var code = ReadCode(root);
        string text = null;
        try
        {
            var p = Path.Combine(root, Path.Combine("share", Path.Combine("locale", Path.Combine(code, "kfwe.txt"))));
            if (code != "ru" && File.Exists(p)) text = File.ReadAllText(p, Encoding.UTF8);
        }
        catch { }
        Use(code, text);
        return true;
    }
}
