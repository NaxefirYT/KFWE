using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace KFMO.Gui;

sealed class MainForm : Form
{
    static readonly Color Bg = Color.FromArgb(30, 30, 32), Panel2 = Color.FromArgb(40, 40, 44), Fg = Color.FromArgb(230, 230, 230),
        Dim = Color.FromArgb(150, 150, 155), Accent = Color.FromArgb(14, 99, 156), Good = Color.FromArgb(110, 200, 120), Bad = Color.FromArgb(235, 110, 100);

    readonly Settings st = Settings.Load();
    readonly TextBox mapBox = new(), suffixBox = new(), gameBox = new(), log = new();
    readonly Label outLabel = new(), result = new();
    readonly CheckBox cSlk = new(), cUnused = new(), cStrip = new(), cJass = new(), cIcons = new(), cKeys = new(), cList = new(), cPreview = new(), cSkins = new();
    readonly ComboBox models = new(), textures = new(), texMax = new();
    readonly Button pick = new(), run = new(), openDir = new(), pickGame = new();
    readonly ProgressBar bar = new();
    readonly ToolTip tip = new() { AutoPopDelay = 20000, InitialDelay = 300 };
    static readonly int[] TexQ = { 0, 90, 85, 75 };
    static readonly int[] TexSizes = { 0, 512, 256, 128 };
    string? lastOutput;

    public MainForm()
    {
        Text = "KFMO";
        BackColor = Bg; ForeColor = Fg;
        Font = new Font("Segoe UI", 9.5f);
        AutoScaleMode = AutoScaleMode.Dpi;
        ClientSize = new Size(760, 640);
        MinimumSize = new Size(640, 560);
        StartPosition = FormStartPosition.CenterScreen;
        AllowDrop = true;
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

        var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(16), BackColor = Bg };
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        Controls.Add(root);

        var title = new Label { Text = "Карта (можно перетащить в окно)", AutoSize = true, ForeColor = Dim, Margin = new Padding(0, 0, 0, 4) };
        root.Controls.Add(title);
        var mapRow = Row(3);
        mapRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        mapRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        mapRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        StyleBox(mapBox); mapBox.Dock = DockStyle.Fill;
        StyleButton(pick, "Выбрать"); pick.Click += (_, _) => PickMap();
        mapRow.Controls.Add(mapBox, 0, 0); mapRow.Controls.Add(pick, 1, 0);
        root.Controls.Add(mapRow);

        var outRow = Row(4);
        outRow.Controls.Add(new Label { Text = "Результат:", AutoSize = true, ForeColor = Dim, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 4, 0) });
        outLabel.AutoSize = true; outLabel.Anchor = AnchorStyles.Left; outLabel.Margin = new Padding(0, 6, 16, 0);
        outRow.Controls.Add(outLabel);
        outRow.Controls.Add(new Label { Text = "Суффикс:", AutoSize = true, ForeColor = Dim, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 4, 0) });
        StyleBox(suffixBox); suffixBox.Width = 70;
        outRow.Controls.Add(suffixBox);
        root.Controls.Add(outRow);

        var opts = new TableLayoutPanel { ColumnCount = 2, AutoSize = true, Dock = DockStyle.Top, BackColor = Panel2, Padding = new Padding(12, 10, 12, 10), Margin = new Padding(0, 12, 0, 8) };
        opts.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        opts.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        Check(cSlk, "Объекты в SLK", "Данные объектов пишутся таблицами SLK. Карта грузится намного быстрее.");
        Check(cUnused, "Удалять неиспользуемые объекты", "Удаляет объекты, на которые нигде нет ссылок.");
        Check(cStrip, "Удалять файлы редактора", "Триггеры, регионы, камеры и т.п. Игре не нужны.\nКарта перестанет нормально открываться в редакторе, храни исходник.");
        Check(cJass, "Сжимать скрипт", "Короткие имена, без комментариев. Проверяется через pjass.");
        Check(cIcons, "Иконки без мипмапов", "Иконкам остаётся только основной размер. Картинка не меняется.");
        Check(cKeys, "Убирать лишние ключи анимации", "Удаляет ключи, которые ничего не меняют.");
        Check(cList, "Сохранять listfile", "Нужен, чтобы карту можно было распаковать.");
        Check(cPreview, "Удалять превью", "Картинка карты в списке карт. Без неё игра покажет миникарту.");
        opts.Controls.Add(cSlk); opts.Controls.Add(cJass);
        opts.Controls.Add(cUnused); opts.Controls.Add(cIcons);
        opts.Controls.Add(cStrip); opts.Controls.Add(cKeys);
        Check(cSkins, "Модели для Reforged", "Вместе с SLK пишет UnitSkin.txt и ItemSkin.txt. Без них в Reforged юниты и предметы невидимы. 1.26 эти файлы не читает.");
        opts.Controls.Add(cList); opts.Controls.Add(cPreview);
        opts.Controls.Add(cSkins);
        cSlk.CheckedChanged += (_, _) => cSkins.Enabled = cSlk.Checked;

        StyleCombo(models, "7 бит", "Бережно", "Не трогать");
        StyleCombo(textures, "Без потерь", "Качество 90", "Качество 85", "Качество 75");
        tip.SetToolTip(models, "Округление чисел в моделях. Модели сжимаются лучше.");
        tip.SetToolTip(textures, "Без потерь: пиксели не меняются.");
        StyleCombo(texMax, "Без ограничения", "512", "256", "128");
        tip.SetToolTip(texMax, "Текстуры моделей больше этого размера уменьшаются вдвое. Интерфейс не трогается.");
        opts.Controls.Add(Labeled("Модели:", models), 0, 5);
        opts.Controls.Add(Labeled("Текстуры:", textures), 1, 5);
        opts.Controls.Add(Labeled("Макс. размер текстур моделей:", texMax), 1, 6);

        var gameRow = Row(3);
        gameRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        gameRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        gameRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        gameRow.Margin = new Padding(0, 8, 0, 0);
        gameRow.Controls.Add(new Label { Text = "Папка игры:", AutoSize = true, ForeColor = Dim, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 6, 0) }, 0, 0);
        StyleBox(gameBox); gameBox.Dock = DockStyle.Fill;
        tip.SetToolTip(gameBox, "Нужна для SLK и проверки скрипта.");
        StyleButton(pickGame, "…"); pickGame.Click += (_, _) => PickGame();
        gameRow.Controls.Add(gameBox, 1, 0); gameRow.Controls.Add(pickGame, 2, 0);
        opts.Controls.Add(gameRow, 0, 7);
        opts.SetColumnSpan(gameRow, 2);
        root.Controls.Add(opts);

        var btnRow = Row(4);
        StyleButton(run, "Оптимизировать"); run.BackColor = Accent; run.Font = new Font(Font.FontFamily, 11f, FontStyle.Bold); run.Padding = new Padding(18, 6, 18, 6);
        run.Click += async (_, _) => await RunAsync();
        StyleButton(openDir, "Открыть папку"); openDir.Enabled = false;
        openDir.Click += (_, _) => { if (lastOutput != null && File.Exists(lastOutput)) Process.Start("explorer.exe", $"/select,\"{lastOutput}\""); };
        bar.Style = ProgressBarStyle.Marquee; bar.MarqueeAnimationSpeed = 0; bar.Width = 160; bar.Anchor = AnchorStyles.Left; bar.Visible = false; bar.Margin = new Padding(12, 10, 0, 0);
        btnRow.Controls.Add(run); btnRow.Controls.Add(openDir); btnRow.Controls.Add(bar);
        root.Controls.Add(btnRow);

        log.Multiline = true; log.ReadOnly = true; log.ScrollBars = ScrollBars.Vertical; log.Dock = DockStyle.Fill;
        log.BackColor = Color.FromArgb(22, 22, 24); log.ForeColor = Color.FromArgb(200, 200, 200); log.BorderStyle = BorderStyle.FixedSingle;
        log.Font = new Font("Consolas", 9f); log.Margin = new Padding(0, 10, 0, 0); log.WordWrap = true;
        root.Controls.Add(log);

        result.AutoSize = true; result.Font = new Font(Font.FontFamily, 10.5f, FontStyle.Bold); result.Margin = new Padding(0, 10, 0, 0);
        root.Controls.Add(result);

        LoadSettings();
        foreach (var c in new Control[] { cSlk, cUnused, cStrip, cJass, cIcons, cKeys, cList, cPreview, cSkins }) ((CheckBox)c).CheckedChanged += (_, _) => SaveSettings();
        models.SelectedIndexChanged += (_, _) => SaveSettings();
        textures.SelectedIndexChanged += (_, _) => SaveSettings();
        texMax.SelectedIndexChanged += (_, _) => SaveSettings();
        suffixBox.TextChanged += (_, _) => { SaveSettings(); UpdateOut(); };
        mapBox.TextChanged += (_, _) => { UpdateOut(); SaveSettings(); };
        gameBox.TextChanged += (_, _) => SaveSettings();
        DragEnter += (_, e) => { if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true) e.Effect = DragDropEffects.Copy; };
        DragDrop += (_, e) => { if (e.Data?.GetData(DataFormats.FileDrop) is string[] f && f.Length > 0) mapBox.Text = f[0]; };
        UpdateOut();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        try { int on = 1; DwmSetWindowAttribute(Handle, 20, ref on, 4); } catch { }
    }

    [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr h, int attr, ref int val, int size);

    static TableLayoutPanel Row(int cols) => new() { ColumnCount = cols, AutoSize = true, Dock = DockStyle.Top, Margin = new Padding(0), BackColor = Color.Transparent };

    void StyleBox(TextBox b) { b.BackColor = Panel2; b.ForeColor = Fg; b.BorderStyle = BorderStyle.FixedSingle; b.Margin = new Padding(0, 2, 6, 2); }

    void StyleButton(Button b, string text)
    {
        b.Text = text; b.AutoSize = true; b.FlatStyle = FlatStyle.Flat; b.BackColor = Panel2; b.ForeColor = Fg;
        b.FlatAppearance.BorderColor = Color.FromArgb(70, 70, 76); b.Padding = new Padding(10, 2, 10, 2); b.Margin = new Padding(0, 0, 8, 0);
        b.Cursor = Cursors.Hand;
    }

    void StyleCombo(ComboBox c, params string[] items)
    {
        c.DropDownStyle = ComboBoxStyle.DropDownList; c.FlatStyle = FlatStyle.Flat; c.BackColor = Panel2; c.ForeColor = Fg;
        c.Items.AddRange(items); c.Width = 320; c.Margin = new Padding(0, 2, 0, 2);
        c.MouseWheel += (_, e) => { if (!c.DroppedDown && e is HandledMouseEventArgs h) h.Handled = true; };
    }

    void Check(CheckBox c, string text, string help)
    {
        c.Text = text; c.AutoSize = true; c.ForeColor = Fg; c.Margin = new Padding(0, 4, 12, 4);
        tip.SetToolTip(c, help);
    }

    Control Labeled(string label, Control c)
    {
        var p = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Margin = new Padding(0, 8, 12, 0), WrapContents = false };
        p.Controls.Add(new Label { Text = label, AutoSize = true, ForeColor = Dim, Margin = new Padding(0, 0, 0, 2) });
        p.Controls.Add(c);
        return p;
    }

    void LoadSettings()
    {
        cSlk.Checked = st.Slk; cUnused.Checked = st.RemoveUnused; cStrip.Checked = st.StripEditor; cJass.Checked = st.JassMin;
        cIcons.Checked = st.IconOneMip; cKeys.Checked = st.KeyReduce; cList.Checked = st.Listfile;
        models.SelectedIndex = Math.Clamp(st.Models, 0, 2);
        textures.SelectedIndex = Math.Max(0, Array.IndexOf(TexQ, st.Textures));
        texMax.SelectedIndex = Math.Max(0, Array.IndexOf(TexSizes, st.TexMax));
        cPreview.Checked = st.NoPreview;
        cSkins.Checked = st.ReforgedSkins; cSkins.Enabled = cSlk.Checked;
        suffixBox.Text = st.Suffix;
        mapBox.Text = st.LastMap;
        gameBox.Text = string.IsNullOrWhiteSpace(st.GameDir) ? GameFiles.DefaultDir() : st.GameDir;
    }

    void SaveSettings()
    {
        st.Slk = cSlk.Checked; st.RemoveUnused = cUnused.Checked; st.StripEditor = cStrip.Checked; st.JassMin = cJass.Checked;
        st.IconOneMip = cIcons.Checked; st.KeyReduce = cKeys.Checked; st.Listfile = cList.Checked;
        st.NoPreview = cPreview.Checked; st.ReforgedSkins = cSkins.Checked; st.TexMax = TexSizes[Math.Max(0, texMax.SelectedIndex)];
        st.Models = Math.Max(0, models.SelectedIndex); st.Textures = TexQ[Math.Max(0, textures.SelectedIndex)];
        st.Suffix = suffixBox.Text; st.LastMap = mapBox.Text; st.GameDir = gameBox.Text;
        st.Save();
    }

    string? OutputPath()
    {
        var p = mapBox.Text.Trim().Trim('"');
        if (p.Length == 0) return null;
        var dir = Path.GetDirectoryName(p) ?? "";
        return Path.Combine(dir, Path.GetFileNameWithoutExtension(p) + suffixBox.Text + Path.GetExtension(p));
    }

    void UpdateOut()
    {
        var o = OutputPath();
        outLabel.Text = o == null ? "" : Path.GetFileName(o);
    }

    void PickMap()
    {
        using var d = new OpenFileDialog { Filter = "Карты Warcraft III (*.w3x;*.w3m)|*.w3x;*.w3m|Все файлы|*.*", Title = "Выберите карту" };
        var cur = mapBox.Text.Trim('"');
        if (cur.Length > 0 && Directory.Exists(Path.GetDirectoryName(cur))) d.InitialDirectory = Path.GetDirectoryName(cur);
        if (d.ShowDialog(this) == DialogResult.OK) mapBox.Text = d.FileName;
    }

    void PickGame()
    {
        using var d = new FolderBrowserDialog { Description = "Папка Warcraft III", UseDescriptionForTitle = true };
        if (Directory.Exists(gameBox.Text)) d.InitialDirectory = gameBox.Text;
        if (d.ShowDialog(this) == DialogResult.OK) gameBox.Text = d.SelectedPath;
    }

    async Task RunAsync()
    {
        var input = mapBox.Text.Trim().Trim('"');
        if (!File.Exists(input)) { Fail("Нет файла: " + input); return; }
        var output = OutputPath()!;
        if (string.Equals(Path.GetFullPath(input), Path.GetFullPath(output), StringComparison.OrdinalIgnoreCase))
        { Fail("Укажи суффикс, иначе результат перезапишет карту."); return; }
        if (!File.Exists(Path.Combine(gameBox.Text, "war3.mpq")) && (cSlk.Checked || cJass.Checked))
            if (MessageBox.Show(this, "В папке игры нет war3.mpq. SLK и сжатие скрипта будут пропущены. Продолжить?", "KFMO", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;

        GameFiles.OverrideDir = gameBox.Text;
        var o = new Optimize.Options
        {
            Input = input, Output = output, Slk = cSlk.Checked, KeepUnused = !cUnused.Checked, ReforgedSkins = cSkins.Checked, StripEditor = cStrip.Checked,
            JassMin = cJass.Checked, IconOneMip = cIcons.Checked, KeyReduce = cKeys.Checked, Listfile = cList.Checked,
            TexQuality = TexQ[Math.Max(0, textures.SelectedIndex)],
            TexMax = TexSizes[Math.Max(0, texMax.SelectedIndex)], NoPreview = cPreview.Checked, SectorShift = 9,
            Quant = models.SelectedIndex switch { 0 => MdxQuant.Bits.Release(), 1 => new MdxQuant.Bits(), _ => null },
        };
        log.Clear(); result.Text = ""; openDir.Enabled = false; lastOutput = null;
        SetBusy(true);
        long inSize = new FileInfo(input).Length;
        var sw = Stopwatch.StartNew();
        int rc = -1; string? error = null;
        var oldOut = Console.Out;
        var writer = new UiWriter(this, log);
        try
        {
            Console.SetOut(writer);
            rc = await Task.Run(() => Optimize.Run(o));
        }
        catch (Exception ex) { error = ex.Message; writer.WriteLine("Ошибка: " + ex); }
        finally { Console.SetOut(oldOut); SetBusy(false); }

        if (rc == 0 && File.Exists(output))
        {
            long outSize = new FileInfo(output).Length;
            result.ForeColor = Good;
            result.Text = $"Готово: {inSize / 1048576.0:F2} > {outSize / 1048576.0:F2} МБ (-{100.0 * (inSize - outSize) / inSize:F1}%), {sw.Elapsed.TotalSeconds:F1} с";
            lastOutput = output; openDir.Enabled = true;
        }
        else Fail(error ?? (rc == 2 ? "В карте есть файлы без имени, нужен listfile." : "Ошибка, см. журнал."));
    }

    void Fail(string msg) { result.ForeColor = Bad; result.Text = msg; }

    void SetBusy(bool busy)
    {
        run.Enabled = !busy; pick.Enabled = !busy; pickGame.Enabled = !busy;
        bar.Visible = busy; bar.MarqueeAnimationSpeed = busy ? 30 : 0;
        Cursor = busy ? Cursors.AppStarting : Cursors.Default;
    }

    sealed class UiWriter : TextWriter
    {
        readonly Control owner; readonly TextBox box; readonly StringBuilder line = new();
        public UiWriter(Control owner, TextBox box) { this.owner = owner; this.box = box; }
        public override Encoding Encoding => Encoding.UTF8;
        public override void Write(char value)
        {
            lock (line)
            {
                if (value == '\r') return;
                if (value != '\n') { line.Append(value); return; }
                var s = line.ToString(); line.Clear();
                owner.BeginInvoke(() => box.AppendText(s + Environment.NewLine));
            }
        }
    }
}
