using WinApi;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace YDColorizer
{
    public partial class EditDialogBox : Form
    {
        /// <summary>
        /// The class name of the tag to search for
        /// </summary>
        private const string StaticClassName = "Static";

        /// <summary>
        /// The class name of the search text box
        /// </summary>
        private const string EditClassName = "Edit";

        /// <summary>
        /// The class name of the search button
        /// </summary>
        private const string ButtonClassName = "Button";

        public  IntPtr hWnd = IntPtr.Zero;
        private IntPtr hStatic;
        private IntPtr hEdit;
        private IntPtr hBtnOk;
        private IntPtr hBtnCancel;

        public EditDialogBox()
        {
            InitializeComponent();
            ApplyModernLook();
        }

        ~EditDialogBox()
        {
            new WinApi.Window(this.hWnd).Show();
        }

        private void EditDialogBox_Load(object sender, EventArgs e)
        {
            this.txtEdit.undoStackChange += new EventHandler((object undoCount, EventArgs empty) => { if ((int)undoCount == 0) { this.btnUndo.Enabled = false; } else if ((int)undoCount > 0) { this.btnUndo.Enabled = true; } });// 根据文本框可撤销数量设置撤销按钮是否可用
            this.txtEdit.redoStackChange += new EventHandler((object redoCount, EventArgs empty) => { if ((int)redoCount == 0) { this.btnRedo.Enabled = false; } else if ((int)redoCount > 0) { this.btnRedo.Enabled = true; } });// 根据文本框可重做数量设置重做按钮是否可用
        }

        public void AttachDialog(IntPtr hWnd)
        {
            this.hWnd = hWnd;// 保存原窗体句柄
            this.ReLoad();// 重新加载控件内容
            this.EnsureOnScreen();
            this.Show();// 显示模拟窗口
            this.Activate();// 激活模拟窗口
            this.txtEdit.Select();// 激活编辑框
        }

        public void DetachDialog(WinApi.Button btn)
        {
            try
            {
                btn.ClickAsync();
            }
            catch (Exception ex)
            {
                MessageBox.Show("If an exception occurs, please take a screenshot and send it to the ydwe forum:" + Environment.NewLine
                    + "Original text box handle:" + hEdit.ToString() + Environment.NewLine
                    + "Confirm button handle:" + hBtnOk.ToString() + Environment.NewLine
                    + "Cancel button handle:" + hBtnCancel.ToString() + Environment.NewLine
                    + "Exception information:" + ex.ToString());
            }

            Config.SaveColor(btn1stColor);
            Config.SaveColor(btn2ndColor);
            Config.SaveColor(btn3rdColor);
            Config.SaveColor(btn4thColor);

            Config.SaveDialogSize(this);

            this.txtEdit.undoStack.Clear();// 清空撤销信息
            this.btnUndo.Enabled = false;
            this.txtEdit.redoStack.Clear();// 清空重做信息
            this.btnRedo.Enabled = false;

            this.Hide();// 隐藏模拟窗口
            //this.Dispose();// 销毁模拟窗口
            this.hWnd = IntPtr.Zero;
        }

        public void ReLoad()
        {
            #region Initialize Tags
            WinApi.Label lblStatic = new WinApi.Label(this.hWnd, IntPtr.Zero, StaticClassName, null);// 搜索原窗口中的标签
            this.hStatic = lblStatic.Handle;// 保存标签句柄
            this.lblStatic.Text = lblStatic.Text;// 从原窗口获取标签文本并设置到模拟窗口
            #endregion

            #region Initialize the text box
            WinApi.TextBox txtEdit = new WinApi.TextBox(this.hWnd, IntPtr.Zero, EditClassName, null);// 搜索原窗口中的文本框
            this.hEdit = txtEdit.Handle;// 保存文本框句柄
            this.txtEdit.lockTextChange = true;// 使文本框进入代码修改模式
            this.txtEdit.Text = txtEdit.Text;// 从原窗口获取文本框文本并设置到模拟窗口
            this.txtEdit.lastText = new MyTextBox.UndoRedoInfo(this.txtEdit.Text, this.txtEdit.SelectionStart);// 设置文本框文本为最后修改的文本
            TextConvert.ConvertToPreviewTextBox(this.txtEdit, this.txtPreview);// 强制将文本转化到预览框
            this.lastFocus = this.txtEdit;// 设置最后焦点在基础文本框
            this.txtEdit.lockTextChange = false;// 离开代码修改模式
            this.txtEdit.SelectAll();// 选取所有文本
            #endregion

            #region Initialize the OK button
            WinApi.Button btnOk = new WinApi.Button(this.hWnd, this.hEdit, ButtonClassName, null);// 搜索原窗口中的确定按钮
            this.hBtnOk = btnOk.Handle;// 保存确定按钮句柄
            this.btnOk.Text = btnOk.Text;// 从原窗口获取确定按钮文本并设置到模拟窗口
            #endregion

            #region Initialize the cancel button
            WinApi.Button btnCancel = new WinApi.Button(this.hWnd, this.hBtnOk, ButtonClassName, null);// 搜索原窗口中的取消按钮
            this.hBtnCancel = btnCancel.Handle;// 保存取消按钮句柄
            this.btnCancel.Text = btnCancel.Text;// 从原窗口获取取消按钮文本并设置到模拟窗口
            #endregion

            #region Initialize Window
            WinApi.Window window = new WinApi.Window(this.hWnd);// 创建原窗口实例
            this.Location = window.Location;// 设置模拟窗口的位置在原窗口的位置上

            if (Config.LoadDialogSize(this) == false)
            {
                this.Width = window.Width;
                this.Height = window.Height * 2;
            }

            this.Text = window.Text;// 从原窗口获取标题并设置到模拟窗口
            window.Hide();// 隐藏原窗口
            #endregion

            #region Initialize the color used by the user
            Config.LoadColor(btn1stColor);
            Config.LoadColor(btn2ndColor);
            Config.LoadColor(btn3rdColor);
            Config.LoadColor(btn4thColor);
            #endregion

            #region Initialization prompt
            #endregion

            //this.Activate();// 激活当前窗口
        }

        private void btnOk_Click(object sender, EventArgs e)
        {
            #region examine'}'
            if (this.txtEdit.Text.Contains("}") == true)// 存在'}'
            {
                if (MessageBox.Show("Текст после символа }:" + Environment.NewLine + this.txtEdit.Text.Substring(this.txtEdit.Text.IndexOf('}')) + Environment.NewLine + "будет потерян редактором. Всё равно сохранить?", "Внимание", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.No)
                {
                    return;
                }
            }
            #endregion

            WinApi.TextBox txtEdit = new WinApi.TextBox(hEdit);
            txtEdit.Text = this.txtEdit.Text;// 写回文本
            WinApi.Button btnOk = new WinApi.Button(hBtnOk);
            DetachDialog(btnOk);
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            WinApi.Button btnCancel = new WinApi.Button(hBtnCancel);
            DetachDialog(btnCancel);
        }

        private void btnUndo_Click(object sender, EventArgs e)
        {
            txtEdit.Undo();
            TextConvert.ConvertToPreviewTextBox(txtEdit, txtPreview);
        }

        private void btnRedo_Click(object sender, EventArgs e)
        {
            txtEdit.Redo();
            TextConvert.ConvertToPreviewTextBox(txtEdit, txtPreview);
        }

        /// <summary>
        /// 选择颜色
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnSelectColor_Click(object sender, EventArgs e)
        {
            System.Windows.Forms.Button btn = sender as System.Windows.Forms.Button;
            ColorPicker colorPicker = new ColorPicker(btn);
            colorPicker.SelectedColorChanged += new EventHandler((object_sender, EventArgs_e) =>
            {
                btn.BackColor = (Color)object_sender;
                colorPicker.Dispose();
                this.Activate();// 使焦点回到对话框上
            });
            colorPicker.Show(btn);
        }

        /// <summary>
        /// 交换按钮颜色
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnSwapColor_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)// 选取颜色
            {
                System.Windows.Forms.Button btn = sender as System.Windows.Forms.Button;
                ColorPicker colorPicker = new ColorPicker(btn);
                colorPicker.SelectedColorChanged += new EventHandler((object_sender, EventArgs_e) =>
                {
                    btn.BackColor = (Color)object_sender;
                    colorPicker.Dispose();
                    this.Activate();// 使焦点回到对话框上
                });
                colorPicker.Show(btn);
            }
            else// 交换颜色
            {
                System.Windows.Forms.Button btn = sender as System.Windows.Forms.Button;
                Color temp = btn.BackColor;
                btn.BackColor = btn1stColor.BackColor;
                btn1stColor.BackColor = temp;
            }
        }

        #region Save last focus
        Control lastFocus;// 最后焦点
        private void txtEdit_Enter(object sender, EventArgs e)
        {
            lastFocus = sender as Control;
        }
        private void txtPreview_Enter(object sender, EventArgs e)
        {
            lastFocus = sender as Control;
        }
        #endregion

        private void txtEdit_TextChanged(object sender, EventArgs e)
        {
            TextConvert.ConvertToPreviewTextBox(txtEdit, txtPreview);
        }

        private void txtPreview_TextChanged(object sender, EventArgs e)
        {
            TextConvert.ConvertToEditTextBox(txtPreview, txtEdit);
        }

        /// <summary>
        /// 设置颜色
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnSetColor_Click(object sender, EventArgs e)
        {
            if (lastFocus is MyTextBox)
            {
                TextConvert.SetTxtEditSingleColor(txtEdit, btn1stColor.BackColor);
                TextConvert.ConvertToPreviewTextBox(txtEdit, txtPreview);
            }
            else if (lastFocus is MyRichTextBox)
            {
                TextConvert.SetTxtPreviewSingleColor(txtPreview, btn1stColor.BackColor);
                TextConvert.ConvertToEditTextBox(txtPreview, txtEdit);
            }
        }

        /// <summary>
        /// 设置渐变
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnGradual_Click(object sender, EventArgs e)
        {
            if (lastFocus is MyTextBox)
            {
                TextConvert.SetTxtEditGradual(txtEdit, btn1stColor.BackColor, btn2ndColor.BackColor);
                TextConvert.ConvertToPreviewTextBox(txtEdit, txtPreview);
            }
            else if (lastFocus is MyRichTextBox)
            {
                TextConvert.SetTxtPreviewGradual(txtPreview, btn1stColor.BackColor, btn2ndColor.BackColor);
                TextConvert.ConvertToEditTextBox(txtPreview, txtEdit);
            }
        }

        /// <summary>
        /// 确定键的快捷键Ctrl+Enter
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void txt_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Control == true && e.KeyCode == Keys.Enter)
            {
                btnOk_Click(btnOk, EventArgs.Empty);
            }
        }

        /// <summary>
        /// 预览框右键菜单
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void txtPreview_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                ContextMenu cm = new ContextMenu();
                cm.MenuItems.Add(new MenuItem("Отменить", new EventHandler((object object_sender, EventArgs EventArgs_e) => { this.txtEdit.Undo(); })) { Enabled = (this.txtEdit.undoStack.Count > 0) });
                cm.MenuItems.Add(new MenuItem("Вырезать", new EventHandler((object object_sender, EventArgs EventArgs_e) => { this.txtPreview.Cut(); })) { Enabled = (this.txtPreview.SelectionLength > 0) });
                cm.MenuItems.Add(new MenuItem("Копировать", new EventHandler((object object_sender, EventArgs EventArgs_e) => { this.txtPreview.Copy(); })) { Enabled = (this.txtPreview.SelectionLength > 0) });
                cm.MenuItems.Add(new MenuItem("Вставить", new EventHandler((object object_sender, EventArgs EventArgs_e) =>
                {
                    this.txtPreview.lockTextChange = true;
                    this.txtPreview.Paste();
                    TextConvert.ConvertToEditTextBox(this.txtPreview, this.txtEdit);
                    this.txtPreview.lockTextChange = false;
                })));
                cm.MenuItems.Add(new MenuItem("Удалить", new EventHandler((object object_sender, EventArgs EventArgs_e) => { this.txtPreview.SelectedText = string.Empty; })) { Enabled = (this.txtPreview.SelectionLength > 0) });
                cm.MenuItems.Add(new MenuItem("Выделить всё", new EventHandler((object object_sender, EventArgs EventArgs_e) => { this.txtPreview.SelectAll(); })));
                cm.Show(this.txtPreview, e.Location);
            }
        }

        private void EditDialogBox_KeyDown(object sender, KeyEventArgs e)// 为撤销和重做按钮添加快捷键
        {
            if (e.Control == true)// 按下Ctrl键
            {
                if (e.KeyCode == Keys.Z)// 按下Z键
                {
                    btnUndo.PerformClick();
                }
                if (e.KeyCode == Keys.Y)// 按下Y键
                {
                    btnRedo.PerformClick();
                }
            }
        }
    }
}