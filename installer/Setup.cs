using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32;

namespace KFWE.Setup;

static class Program
{
    [STAThread]
    static int Main(string[] args)
    {
        if (args.Length == 2 && args[0] == "/silent")
        {
            try { Installer.Install(args[1], null); return 0; }
            catch { return 1; }
        }
        if (args.Length == 3 && args[0] == "/update")
            return Installer.Update(args[1], int.TryParse(args[2], out var pid) ? pid : 0);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new SetupForm());
        return 0;
    }
}

static class Installer
{
    static readonly string[] Keep =
    {
        "bin/EverConfig.cfg", "plugin/warcraft3/config.cfg", "plugin/w3x2lni/config.ini",
        "plugin/ydhost/map.cfg", "plugin/ydhost/ydhost.cfg", "plugin/YDColorizer/YDColorizerConfig.ini",
    };

    static readonly string[] Obsolete = { "bin/splash.bmp" };

    public static string FindWar3()
    {
        foreach (var hive in new[] { RegistryHive.CurrentUser, RegistryHive.LocalMachine })
            foreach (var view in new[] { RegistryView.Registry32, RegistryView.Registry64 })
            {
                using var key = RegistryKey.OpenBaseKey(hive, view).OpenSubKey(@"Software\Blizzard Entertainment\Warcraft III");
                if (key?.GetValue("InstallPath") is string dir && dir.Trim().Length > 0 && IsWar3(dir))
                    return Path.GetFullPath(dir).TrimEnd('\\');
            }
        return "";
    }

    public static bool IsWar3(string dir) => Directory.Exists(dir) && File.Exists(Path.Combine(dir, "game.dll"));

    public static string GameVersion(string dir) =>
        (FileVersionInfo.GetVersionInfo(Path.Combine(dir, "game.dll")).FileVersion ?? "").Replace(", ", ".");

    public static bool EditorRunning() =>
        Process.GetProcessesByName("worldeditydwe").Length > 0 || Process.GetProcessesByName("YDWEConfig").Length > 0;

    public static void Install(string target, IProgress<int>? progress)
    {
        using var payload = Assembly.GetExecutingAssembly().GetManifestResourceStream("kfwe.zip")
            ?? throw new InvalidOperationException("Установщик собран без файлов KFWE.");
        using var zip = new ZipArchive(payload, ZipArchiveMode.Read);
        int done = 0;
        foreach (var e in zip.Entries)
        {
            var dst = Path.Combine(target, e.FullName.Replace('/', '\\'));
            if (e.FullName.EndsWith("/"))
                Directory.CreateDirectory(dst);
            else if (!(Keep.Contains(e.FullName, StringComparer.OrdinalIgnoreCase) && File.Exists(dst)))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
                if (File.Exists(dst)) File.SetAttributes(dst, FileAttributes.Normal);
                e.ExtractToFile(dst, true);
            }
            progress?.Report(++done * 100 / zip.Entries.Count);
        }
        foreach (var f in Obsolete)
        {
            var path = Path.Combine(target, f.Replace('/', '\\'));
            if (File.Exists(path)) File.Delete(path);
        }
    }

    public static int Update(string target, int pid)
    {
        try { if (pid > 0) Process.GetProcessById(pid).WaitForExit(); }
        catch (ArgumentException) { }
        while (EditorRunning()) System.Threading.Thread.Sleep(500);
        try
        {
            Install(target, null);
        }
        catch (Exception ex)
        {
            MessageBox.Show("Не удалось установить обновление: " + ex.Message + "\n\nСкачайте KFWE-Setup.exe со страницы релизов и запустите вручную.",
                "Обновление KFWE", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return 1;
        }
        var exe = Path.Combine(target, "YDWE.exe");
        if (File.Exists(exe)) Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = target });
        return 0;
    }

    public static void Shortcut(string exe)
    {
        var lnkPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "KFWE.lnk");
        var shellType = Type.GetTypeFromProgID("WScript.Shell")!;
        var shell = Activator.CreateInstance(shellType);
        var lnk = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { lnkPath })!;
        var t = lnk.GetType();
        t.InvokeMember("TargetPath", BindingFlags.SetProperty, null, lnk, new object[] { exe });
        t.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, lnk, new object[] { Path.GetDirectoryName(exe)! });
        t.InvokeMember("Save", BindingFlags.InvokeMethod, null, lnk, null);
    }
}

sealed class SetupForm : Form
{
    static readonly Color CBg = Color.FromArgb(0x1E, 0x1F, 0x24), CField = Color.FromArgb(0x2C, 0x2D, 0x33),
        CText = Color.FromArgb(0xE4, 0xE6, 0xEB), CDim = Color.FromArgb(0x8B, 0x90, 0xA0),
        CAccent = Color.FromArgb(0x7C, 0x5C, 0xFF), CErr = Color.FromArgb(0xFF, 0x7A, 0x7A);

    readonly TextBox path = new();
    readonly Label hint = new();
    readonly CheckBox shortcut = new();
    readonly Panel bar = new(), fill = new();
    readonly Button browse, install, cancel;
    string? exe;

    [DllImport("dwmapi.dll")]
    static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

    public SetupForm()
    {
        Text = "Установка KFWE";
        Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = CBg;
        ForeColor = CText;
        Font = new Font("Segoe UI", 9.75f);
        ClientSize = new Size(520, 252);

        var title = new Label { Text = "KFWE 1.5.0", Font = new Font("Segoe UI Semibold", 18f), AutoSize = true, Location = new Point(22, 16) };
        var sub = new Label { Text = "Редактор Warcraft 3 нового поколения", ForeColor = CDim, AutoSize = true, Location = new Point(25, 54) };
        var cap = new Label { Text = "Папка Warcraft III", AutoSize = true, Location = new Point(24, 94) };

        path.SetBounds(24, 118, 372, 27);
        path.BackColor = CField;
        path.ForeColor = CText;
        path.BorderStyle = BorderStyle.FixedSingle;
        path.TextChanged += (_, _) => Check();

        browse = Flat("Обзор…", CField);
        browse.SetBounds(404, 117, 92, 27);
        browse.Click += (_, _) => Browse();

        hint.SetBounds(24, 150, 472, 20);

        shortcut.Text = "Ярлык на рабочем столе";
        shortcut.Checked = true;
        shortcut.AutoSize = true;
        shortcut.Location = new Point(24, 178);

        bar.SetBounds(24, 184, 472, 6);
        bar.BackColor = CField;
        bar.Visible = false;
        fill.SetBounds(0, 0, 0, 6);
        fill.BackColor = CAccent;
        bar.Controls.Add(fill);

        install = Flat("Установить", CAccent);
        install.ForeColor = Color.White;
        install.SetBounds(288, 206, 116, 32);
        install.Click += async (_, _) => await Run();

        cancel = Flat("Отмена", CField);
        cancel.SetBounds(412, 206, 84, 32);
        cancel.Click += (_, _) => Close();

        Controls.AddRange(new Control[] { title, sub, cap, path, browse, hint, shortcut, bar, install, cancel });
        AcceptButton = install;
        path.Text = Installer.FindWar3();
        Check();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        int on = 1;
        DwmSetWindowAttribute(Handle, 20, ref on, 4);
    }

    static Button Flat(string text, Color back)
    {
        var b = new Button { Text = text, FlatStyle = FlatStyle.Flat, BackColor = back, ForeColor = CText, Cursor = Cursors.Hand, UseVisualStyleBackColor = false };
        b.FlatAppearance.BorderSize = 0;
        b.FlatAppearance.MouseOverBackColor = ControlPaint.Light(back, 0.15f);
        return b;
    }

    void Hint(string text, bool error)
    {
        hint.Text = text;
        hint.ForeColor = error ? CErr : CDim;
    }

    void Check()
    {
        if (exe != null) return;
        var dir = path.Text.Trim();
        install.Enabled = Installer.IsWar3(dir);
        if (dir.Length == 0)
            Hint("Укажите папку с игрой, в ней должен лежать game.dll", true);
        else if (!install.Enabled)
            Hint("В этой папке нет game.dll", true);
        else if (!Installer.GameVersion(dir).StartsWith("1.26"))
            Hint("Нужна версия 1.26a, у вас " + Installer.GameVersion(dir) + ". Редактор может не запуститься", true);
        else if (Directory.Exists(Path.Combine(dir, "KFWE")))
            Hint("KFWE уже установлен, файлы обновятся, а настройки останутся", false);
        else
            Hint("Редактор будет установлен в " + Path.Combine(dir, "KFWE"), false);
    }

    void Browse()
    {
        using var dlg = new FolderBrowserDialog { Description = "Папка Warcraft III", SelectedPath = path.Text, ShowNewFolderButton = false };
        if (dlg.ShowDialog(this) == DialogResult.OK) path.Text = dlg.SelectedPath;
    }

    async Task Run()
    {
        if (exe != null)
        {
            Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = Path.GetDirectoryName(exe)! });
            Close();
            return;
        }
        if (Installer.EditorRunning())
        {
            MessageBox.Show(this, "Закройте редактор и нажмите «Установить» ещё раз.", Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        var target = Path.Combine(path.Text.Trim(), "KFWE");
        path.Enabled = browse.Enabled = install.Enabled = cancel.Enabled = false;
        shortcut.Visible = false;
        bar.Visible = true;
        Hint("Устанавливаю…", false);
        var progress = new Progress<int>(p => fill.Width = bar.Width * p / 100);
        try
        {
            await Task.Run(() => Installer.Install(target, progress));
        }
        catch (Exception ex)
        {
            Hint("Не получилось: " + ex.Message, true);
            path.Enabled = browse.Enabled = install.Enabled = cancel.Enabled = true;
            shortcut.Visible = true;
            bar.Visible = false;
            return;
        }
        exe = Path.Combine(target, "YDWE.exe");
        Hint("Готово", false);
        if (shortcut.Checked)
        {
            try { Installer.Shortcut(exe); }
            catch { Hint("Готово, но ярлык создать не получилось", true); }
        }
        install.Text = "Запустить";
        cancel.Text = "Закрыть";
        install.Enabled = cancel.Enabled = true;
    }
}
