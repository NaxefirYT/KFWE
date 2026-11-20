using System.Globalization;

namespace KFMO.Gui;

static class Program
{
    [STAThread]
    static void Main()
    {
        InitLang();
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }

    static void InitLang()
    {
        if (KfLang.LoadFrom(AppContext.BaseDirectory)) return;
        var sys = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
        if (sys is "ru" or "uk" or "be" or "kk") return;
        var asm = typeof(Program).Assembly;
        var langs = asm.GetManifestResourceNames().Where(n => n.StartsWith("lang/")).ToDictionary(n => n[5..].TrimEnd('\\', '/'), n => n);
        var code = langs.Keys.FirstOrDefault(l => l == sys || l.StartsWith(sys + "-")) ?? langs.Keys.FirstOrDefault(l => l.StartsWith("en"));
        if (code == null) return;
        using var s = asm.GetManifestResourceStream(langs[code])!;
        using var r = new StreamReader(s);
        KfLang.Use(code, r.ReadToEnd());
    }
}
