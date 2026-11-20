using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace YDColorizer
{
    public partial class EditDialogBox
    {
        private static readonly Color ClrBg = Color.FromArgb(0x1E, 0x1F, 0x22);
        private static readonly Color ClrText = Color.FromArgb(0xE8, 0xE8, 0xEA);
        private static readonly Color ClrSubtext = Color.FromArgb(0x9A, 0x9C, 0xA3);
        private static readonly Color ClrBorder = Color.FromArgb(0x3C, 0x3E, 0x44);
        private static readonly Color ClrButton = Color.FromArgb(0x34, 0x36, 0x3B);
        private static readonly Color ClrButtonHot = Color.FromArgb(0x40, 0x43, 0x4A);
        private static readonly Color ClrAccent = Color.FromArgb(0x4C, 0x8D, 0xF6);
        private static readonly Color ClrAccentHot = Color.FromArgb(0x63, 0x9E, 0xF8);
        private static readonly Color ClrEditBg = Color.FromArgb(0x17, 0x18, 0x1B);
        private static readonly Color ClrTooltipBg = Color.FromArgb(0x0B, 0x0D, 0x12);

        private struct Swatch
        {
            public Color Color;
            public string Name;
            public Swatch(int rgb, string name) { Color = Color.FromArgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); Name = name; }
        }

        private static readonly Swatch[] Palette =
        {
            new Swatch(0xFF0303, KfLang.T("Красный (игрок 1)")), new Swatch(0x0042FF, KfLang.T("Синий (игрок 2)")),
            new Swatch(0x1CE6B9, KfLang.T("Бирюзовый (игрок 3)")), new Swatch(0x540081, KfLang.T("Фиолетовый (игрок 4)")),
            new Swatch(0xFFFC01, KfLang.T("Жёлтый (игрок 5)")), new Swatch(0xFE8A0E, KfLang.T("Оранжевый (игрок 6)")),
            new Swatch(0x20C000, KfLang.T("Зелёный (игрок 7)")), new Swatch(0xE55BB0, KfLang.T("Розовый (игрок 8)")),
            new Swatch(0x959697, KfLang.T("Серый (игрок 9)")), new Swatch(0x7EBFF1, KfLang.T("Голубой (игрок 10)")),
            new Swatch(0x106246, KfLang.T("Тёмно-зелёный (игрок 11)")), new Swatch(0x4E2A04, KfLang.T("Коричневый (игрок 12)")),
            new Swatch(0xFFCC00, KfLang.T("Золотой (горячие клавиши)")), new Swatch(0xFFFFFF, KfLang.T("Белый")),
            new Swatch(0xC0C0C0, KfLang.T("Светло-серый")), new Swatch(0x00FF00, KfLang.T("Ярко-зелёный")),
            new Swatch(0xFF0000, KfLang.T("Ярко-красный")), new Swatch(0x00FFFF, KfLang.T("Циан")),
            new Swatch(0x4080FF, KfLang.T("Голубой (мана)")), new Swatch(0xFF8000, KfLang.T("Оранжевый")),
        };

        private ToolTip modernTips;
        private Label lblPalette;
        private Panel palettePanel;
        private Label lblHint;

        [DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        private static extern int SetWindowTheme(IntPtr hWnd, string pszSubAppName, string pszSubIdList);

        private void ApplyModernLook()
        {
            this.SuspendLayout();

            this.Font = new Font("Segoe UI", 9.75f);
            this.BackColor = ClrBg;
            this.ForeColor = ClrText;
            this.FormBorderStyle = FormBorderStyle.Sizable;
            this.MinimumSize = new Size(720, 500);
            this.Padding = new Padding(0);

            this.lblStatic.ForeColor = ClrSubtext;
            this.lblStatic.AutoSize = true;

            this.splitContainer.BackColor = ClrBorder;
            this.splitContainer.SplitterWidth = 6;
            this.splitContainer.Panel1.BackColor = ClrEditBg;
            this.splitContainer.Panel1.Padding = new Padding(8, 6, 4, 6);
            this.splitContainer.Panel2.BackColor = ClrTooltipBg;
            this.splitContainer.Panel2.Padding = new Padding(10, 8, 4, 8);
            this.txtEdit.BorderStyle = BorderStyle.None;
            this.txtEdit.BackColor = ClrEditBg;
            this.txtEdit.ForeColor = ClrText;
            this.txtEdit.Font = new Font("Consolas", 11f);
            this.txtPreview.BorderStyle = BorderStyle.None;
            this.txtPreview.BackColor = ClrTooltipBg;
            this.txtPreview.ForeColor = Color.White;
            this.txtPreview.Font = new Font("Segoe UI Semibold", 11f);
            this.txtEdit.HandleCreated += (s, e) => SetWindowTheme(this.txtEdit.Handle, "DarkMode_Explorer", null);
            this.txtPreview.HandleCreated += (s, e) => SetWindowTheme(this.txtPreview.Handle, "DarkMode_Explorer", null);

            StyleButton(this.btnOk, true);
            StyleButton(this.btnCancel, false);
            StyleButton(this.btnSetColor, false);
            StyleButton(this.btnGradual, false);
            StyleButton(this.btnUndo, false);
            StyleButton(this.btnRedo, false);
            this.btnSetColor.Image = null;
            this.btnSetColor.Text = KfLang.T("Покрасить");
            this.btnGradual.Image = null;
            this.btnGradual.Text = KfLang.T("Градиент");
            this.btnUndo.Image = null;
            this.btnUndo.Text = "↶";
            this.btnRedo.Image = null;
            this.btnRedo.Text = "↷";
            this.btnUndo.Font = this.btnRedo.Font = new Font("Segoe UI Symbol", 12f);

            foreach (Button b in new Button[] { this.btn1stColor, this.btn2ndColor, this.btn3rdColor, this.btn4thColor })
            {
                StyleSwatch(b);
            }

            this.lblPalette = new Label { Text = KfLang.T("Палитра WC3"), AutoSize = true, ForeColor = ClrSubtext, BackColor = Color.Transparent };
            this.palettePanel = new Panel { BackColor = Color.Transparent };
            foreach (Swatch sw in Palette)
            {
                Button b = new Button { BackColor = sw.Color, Size = new Size(24, 24), TabStop = false, Tag = sw.Name };
                StyleSwatch(b);
                b.MouseUp += PaletteSwatch_MouseUp;
                this.palettePanel.Controls.Add(b);
            }
            this.lblHint = new Label
            {
                AutoSize = true,
                ForeColor = ClrSubtext,
                BackColor = Color.Transparent,
                Text = KfLang.T("Выделите текст и нажмите на цвет. ПКМ по цвету: конец градиента. Ctrl+Enter: перенос строки.")
            };
            this.Controls.Add(this.lblPalette);
            this.Controls.Add(this.palettePanel);
            this.Controls.Add(this.lblHint);

            foreach (Control c in this.Controls)
            {
                c.Anchor = AnchorStyles.Top | AnchorStyles.Left;
            }

            this.modernTips = new ToolTip();
            this.modernTips.SetToolTip(this.btn1stColor, KfLang.T("Основной цвет. Нажмите, чтобы выбрать"));
            foreach (Button b in new Button[] { this.btn2ndColor, this.btn3rdColor, this.btn4thColor })
            {
                this.modernTips.SetToolTip(b, KfLang.T("ЛКМ: выбрать цвет\nПКМ: поменять местами с основным"));
            }
            this.modernTips.SetToolTip(this.btnSetColor, KfLang.T("Покрасить выделенный текст основным цветом"));
            this.modernTips.SetToolTip(this.btnGradual, KfLang.T("Градиент по выделенному тексту: от основного цвета к первому дополнительному"));
            this.modernTips.SetToolTip(this.btnUndo, KfLang.T("Отменить (Ctrl+Z)"));
            this.modernTips.SetToolTip(this.btnRedo, KfLang.T("Повторить (Ctrl+Y)"));
            foreach (Control c in this.palettePanel.Controls)
            {
                this.modernTips.SetToolTip(c, (string)c.Tag + KfLang.T("\nЛКМ: покрасить выделение, ПКМ: конец градиента"));
            }

            this.Resize += (s, e) => LayoutModern();
            this.HandleCreated += (s, e) =>
            {
                int on = 1;
                if (DwmSetWindowAttribute(this.Handle, 20, ref on, 4) != 0)
                {
                    DwmSetWindowAttribute(this.Handle, 19, ref on, 4);
                }
            };

            this.ResumeLayout(false);
            LayoutModern();
        }

        private static void StyleButton(Button b, bool primary)
        {
            b.FlatStyle = FlatStyle.Flat;
            b.UseVisualStyleBackColor = false;
            b.BackColor = primary ? ClrAccent : ClrButton;
            b.ForeColor = primary ? Color.White : ClrText;
            b.FlatAppearance.BorderColor = primary ? ClrAccent : ClrBorder;
            b.FlatAppearance.BorderSize = 1;
            b.FlatAppearance.MouseOverBackColor = primary ? ClrAccentHot : ClrButtonHot;
            b.FlatAppearance.MouseDownBackColor = primary ? ClrAccent : ClrBorder;
            b.Cursor = Cursors.Hand;
        }

        private static void StyleSwatch(Button b)
        {
            b.FlatStyle = FlatStyle.Flat;
            b.UseVisualStyleBackColor = false;
            b.Text = string.Empty;
            b.FlatAppearance.BorderSize = 1;
            b.FlatAppearance.BorderColor = ClrBorder;
            b.Cursor = Cursors.Hand;
            SyncSwatchHover(b);
            b.BackColorChanged += (s, e) => SyncSwatchHover((Button)s);
            b.MouseEnter += (s, e) => ((Button)s).FlatAppearance.BorderColor = Color.White;
            b.MouseLeave += (s, e) => ((Button)s).FlatAppearance.BorderColor = ClrBorder;
        }

        private static void SyncSwatchHover(Button b)
        {
            b.FlatAppearance.MouseOverBackColor = b.BackColor;
            b.FlatAppearance.MouseDownBackColor = b.BackColor;
        }

        private void PaletteSwatch_MouseUp(object sender, MouseEventArgs e)
        {
            Color color = ((Button)sender).BackColor;
            if (e.Button == MouseButtons.Right)
            {
                this.btn2ndColor.BackColor = color;
                return;
            }
            if (e.Button != MouseButtons.Left)
            {
                return;
            }
            this.btn1stColor.BackColor = color;
            btnSetColor_Click(this.btnSetColor, EventArgs.Empty);
            if (this.lastFocus != null)
            {
                this.lastFocus.Focus();
            }
        }

        private void LayoutModern()
        {
            if (this.palettePanel == null)
            {
                return;
            }
            const int m = 16;
            int w = this.ClientSize.Width, h = this.ClientSize.Height;

            this.btnRedo.SetBounds(w - m - 34, 8, 34, 30);
            this.btnUndo.SetBounds(w - m - 34 - 6 - 34, 8, 34, 30);
            this.lblStatic.Location = new Point(m, 14);
            this.lblStatic.MaximumSize = new Size(Math.Max(100, this.btnUndo.Left - m - 8), 0);

            int rowY = h - m - 34;
            this.btn1stColor.SetBounds(m, rowY, 34, 34);
            this.btn2ndColor.SetBounds(m + 42, rowY + 6, 22, 22);
            this.btn3rdColor.SetBounds(m + 70, rowY + 6, 22, 22);
            this.btn4thColor.SetBounds(m + 98, rowY + 6, 22, 22);
            this.btnSetColor.SetBounds(m + 132, rowY, 110, 34);
            this.btnGradual.SetBounds(m + 250, rowY, 100, 34);
            this.btnCancel.SetBounds(w - m - 100, rowY, 100, 34);
            this.btnOk.SetBounds(w - m - 100 - 8 - 100, rowY, 100, 34);

            this.lblHint.Location = new Point(m, rowY - 26);
            int palY = rowY - 26 - 8 - 26;
            this.lblPalette.Location = new Point(m, palY + 4);
            int px = 0;
            foreach (Control c in this.palettePanel.Controls)
            {
                c.Location = new Point(px, 0);
                px += c.Width + 4;
            }
            this.palettePanel.SetBounds(m + this.lblPalette.Width + 10, palY + 1, Math.Max(0, w - 2 * m - this.lblPalette.Width - 10), 24);

            int top = 46;
            this.splitContainer.SetBounds(m, top, w - 2 * m, Math.Max(80, palY - 10 - top));
        }

        private void EnsureOnScreen()
        {
            Rectangle area = Screen.FromPoint(this.Location).WorkingArea;
            int x = Math.Max(area.Left, Math.Min(this.Left, area.Right - this.Width));
            int y = Math.Max(area.Top, Math.Min(this.Top, area.Bottom - this.Height));
            this.Location = new Point(x, y);
        }
    }
}
