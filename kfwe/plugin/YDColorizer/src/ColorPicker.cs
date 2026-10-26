using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace YDColorizer
{
    public partial class ColorPicker : Form
    {
        /// <summary>
        /// Parent Control
        /// </summary>
        private Control parentControl;

        /// <summary>
        /// Selected Color
        /// </summary>
        public Color selectedColor;

        /// <summary>
        /// Color Blocks
        /// </summary>
        private Button[,] btnColor = new Button[5, 8];

        private Button btnOtherColor;

        private Button btnScreenColor;

        /// <summary>
        /// Color is selected
        /// </summary>
        public event EventHandler SelectedColorChanged;

        struct ColorSet
        {
            public Color color;
            public string name;
        }

        /// <summary>
        /// Color set for Simplified Chinese
        /// </summary>
        private ColorSet[,] colorSetChs =
{{new ColorSet{color=Color.FromArgb(  0,  0,  0),name="Black"  },new ColorSet{color=Color.FromArgb(153, 51,  0),name="Brown"},new ColorSet{color=Color.FromArgb( 51, 51,  0),name="Olive"},new ColorSet{color=Color.FromArgb(  0, 51,  0),name="Dark Green"},new ColorSet{color=Color.FromArgb(  0, 51,102),name="Dark Blue"  },new ColorSet{color=Color.FromArgb(  0,  0,128),name="Dark Blue"},new ColorSet{color=Color.FromArgb( 51, 51,153),name="Indigo"  },new ColorSet{color=Color.FromArgb( 51, 51, 51),name="Gray - 80%"}}
,{new ColorSet{color=Color.FromArgb(128,  0,  0),name="Deep Red"  },new ColorSet{color=Color.FromArgb(255,102,  0),name="Orange"},new ColorSet{color=Color.FromArgb(128,128,  0),name="Dark Yellow"  },new ColorSet{color=Color.FromArgb(  0,128,  0),name="Green"},new ColorSet{color=Color.FromArgb(  0,128,128),name="Blue"  },new ColorSet{color=Color.FromArgb(  0,  0,255),name="Blue"},new ColorSet{color=Color.FromArgb(102,102,153),name="Blue Gray"  },new ColorSet{color=Color.FromArgb(128,128,128),name="Gray - 50%"}}
,{new ColorSet{color=Color.FromArgb(255,  0,  0),name="Red"  },new ColorSet{color=Color.FromArgb(255,153,  0),name="Light Orange"},new ColorSet{color=Color.FromArgb(153,204,  0),name="Lime"  },new ColorSet{color=Color.FromArgb( 51,153,102),name="Sea Green"},new ColorSet{color=Color.FromArgb( 51,204,204),name="Water Green"  },new ColorSet{color=Color.FromArgb( 51,102,255),name="Light blue"},new ColorSet{color=Color.FromArgb(128,  0,128),name="Violet"},new ColorSet{color=Color.FromArgb(153,153,153),name="Gray-40%"}}
,{new ColorSet{color=Color.FromArgb(255,  0,255),name="Pink"  },new ColorSet{color=Color.FromArgb(255,104,  0),name="Gold"},new ColorSet{color=Color.FromArgb(255,255,  0),name="Yellow"  },new ColorSet{color=Color.FromArgb(  0,255,  0),name="Bright Green"},new ColorSet{color=Color.FromArgb(  0,255,255),name="Green"  },new ColorSet{color=Color.FromArgb(  0,204,255),name="Sky Blue"},new ColorSet{color=Color.FromArgb(153, 51,102),name="Mei Hong"  },new ColorSet{color=Color.FromArgb(192,192,192),name="Gray-25%"}}
,{new ColorSet{color=Color.FromArgb(255,153,204),name="Rose Red"},new ColorSet{color=Color.FromArgb(255,204,153),name="Light Brown"},new ColorSet{color=Color.FromArgb(255,255,204),name="Light Yellow"  },new ColorSet{color=Color.FromArgb(204,255,204),name="Light green"},new ColorSet{color=Color.FromArgb(204,255,255),name="Light green"},new ColorSet{color=Color.FromArgb(153,204,255),name="Light blue"},new ColorSet{color=Color.FromArgb(204,153,255),name="Light purple"  },new ColorSet{color=Color.FromArgb(255,255,255),name="White"    }}
};

        /// <summary>
        /// Traditional Chinese color set
        /// </summary>
        private ColorSet[,] colorSetCht =
{{new ColorSet{color=Color.FromArgb( 0, 0, 0),name="Black" },new ColorSet{color=Color.FromArgb(153, 51, 0),name="Brown"},new ColorSet {color=Color.FromArgb( 51, 51, 0),name="Olive"},new ColorSet{color=Color.FromArgb( 0, 51, 0),name="Dark Green"},new ColorSet{color =Color.FromArgb( 0, 51,102),name="Dark Blue" },new ColorSet{color=Color.FromArgb( 0, 0,128),name="Dark Blue"},new ColorSet{color=Color.FromArgb( 51, 51,153 ),name="Indigo" },new ColorSet{color=Color.FromArgb( 51, 51, 51),name="Gray - 80%"}}
,{new ColorSet{color=Color.FromArgb(128, 0, 0),name="Deep Red" },new ColorSet{color=Color.FromArgb(255,102, 0),name="Orange"},new ColorSet{ color=Color.FromArgb(128,128, 0),name="Dark Yellow" },new ColorSet{color=Color.FromArgb( 0,128, 0),name="Green"},new ColorSet{color=Color.FromArgb( 0,128,128 ),name="Blue" },new ColorSet{color=Color.FromArgb( 0, 0,255),name="Blue"},new ColorSet{color=Color.FromArgb(102,102,153),name="Blue Gray" } ,new ColorSet{color=Color.FromArgb(128,128,128),name="Gray - 50%"}}
,{new ColorSet{color=Color.FromArgb(255, 0, 0),name="Red" },new ColorSet{color=Color.FromArgb(255,153, 0),name="Light Orange"},new ColorSet{ color=Color.FromArgb(153,204, 0),name="Lime" },new ColorSet{color=Color.FromArgb( 51,153,102),name="Sea Green"},new ColorSet{color=Color.FromArgb( 51,204,204) ,name="Water Green" },new ColorSet{color=Color.FromArgb( 51,102,255),name="Light blue"},new ColorSet{color=Color.FromArgb(128, 0,128),name="Violet"},new ColorSet{color=Color.FromArgb(153,153,153),name="Gray-40%"}}
,{new ColorSet{color=Color.FromArgb(255, 0,255),name="Pink" },new ColorSet{color=Color.FromArgb(255,104, 0),name="Gold"},new ColorSet{color=Color .FromArgb(255,255, 0),name="Yellow" },new ColorSet{color=Color.FromArgb( 0,255, 0),name="Bright Green"},new ColorSet{color=Color.FromArgb( 0,255,255),name ="Green" },new ColorSet{color=Color.FromArgb( 0,204,255),name="Sky Blue"},new ColorSet{color=Color.FromArgb(153, 51,102),name="Mei Hong" },new ColorSet{ color=Color.FromArgb(192,192,192),name="Gray-25%"}}
,{new ColorSet{color=Color.FromArgb(255,153,204),name="Rose Red"},new ColorSet{color=Color.FromArgb(255,204,153),name="Light Brown"},new ColorSet{color=Color.FromArgb( 255,255,204),name="Light Yellow" },new ColorSet{color=Color.FromArgb(204,255,204),name="Light green"},new ColorSet{color=Color.FromArgb(204,255,255),name="Light green"}, new ColorSet{color=Color.FromArgb(153,204,255),name="Light blue"},new ColorSet{color=Color.FromArgb(204,153,255),name="Light purple" },new ColorSet{color=Color.FromArgb(255,255,255 ),name="White" }}
};

        public ColorPicker(Control parentControl)
        {
            InitializeComponent();
            this.parentControl = parentControl;// Save the form handle
            Init();
        }

        private void Init()
        {
            #region Save original color
            this.selectedColor = this.parentControl.BackColor;
            #endregion
            #region Set the parent form
            this.Owner = this.parentControl.TopLevelControl as Form;
            #endregion
            InitLocation();
            InitControls();
        }

        private void InitLocation()
        {
            this.Left = parentControl.TopLevelControl.PointToScreen(parentControl.Location).X;
            this.Top = parentControl.TopLevelControl.PointToScreen(parentControl.Location).Y + parentControl.Height;
        }

        private void InitControls()
        {
            ToolTip toolTip = new ToolTip();

            #region Container
            Panel pnl = new Panel();
            pnl.Dock = DockStyle.Fill;// Fill in the parent container
            pnl.BorderStyle = BorderStyle.FixedSingle;// Set border
            this.Controls.Add(pnl);
            #endregion

            #region Color Blocks
            for (int i = 0; i < 5; i++)
            {
                for (int j = 0; j < 8; j++)
                {
                    btnColor[i, j] = new Button();
                    btnColor[i, j].Left = j * 18 + 6;
                    btnColor[i, j].Top = i * 18 + 6;
                    btnColor[i, j].Width = 12;
                    btnColor[i, j].Height = 12;
                    btnColor[i, j].FlatStyle = FlatStyle.Flat;// Set the button style
                    btnColor[i, j].TabStop = false;// Disable the use of Tab to gain focus
                    btnColor[i, j].BackColor = colorSetChs[i, j].color;// Setting Color
                    btnColor[i, j].Click += new EventHandler(btn_Click);
                    toolTip.SetToolTip(btnColor[i, j], colorSetChs[i, j].name + Environment.NewLine + "(" + colorSetChs[i, j].color.R + "," + colorSetChs[i, j].color.G + "," + colorSetChs[i, j].color.B + ")");// Set description text
                    pnl.Controls.Add(btnColor[i, j]);
                }
            }
            #endregion

            #region Other colors
            btnOtherColor = new Button();
            btnOtherColor.Text = "Другой цвет…";
            toolTip.SetToolTip(btnOtherColor, "Выбрать любой цвет");
            btnOtherColor.Left = 6;
            btnOtherColor.Top = 96;
            btnOtherColor.Width = 138;
            btnOtherColor.Height = 23;
            btnOtherColor.FlatStyle = FlatStyle.Flat;// Set the button style
            btnOtherColor.TabStop = false;// Disable the use of Tab to gain focus

            btnOtherColor.Image = Properties.Resources.icoCustomColor;
            btnOtherColor.ImageAlign = ContentAlignment.MiddleLeft;

            btnOtherColor.Click += new EventHandler(btn_Click);

            pnl.Controls.Add(btnOtherColor);
            #endregion
            #region Screen color
            btnScreenColor = new Button();
            btnScreenColor.Text = "С экрана";
            toolTip.SetToolTip(btnScreenColor, "Взять цвет точки на экране");
            btnScreenColor.Left = 6;
            btnScreenColor.Top = 125;
            btnScreenColor.Width = 138;
            btnScreenColor.Height = 23;
            btnScreenColor.FlatStyle = FlatStyle.Flat;// Set the button style
            btnScreenColor.TabStop = false;// Disable the use of Tab to gain focus

            btnScreenColor.Image = Properties.Resources.icoGetColorFromScreen;
            btnScreenColor.ImageAlign = ContentAlignment.MiddleLeft;

            btnScreenColor.Click += new EventHandler(btn_Click);

            pnl.Controls.Add(btnScreenColor);
            #endregion
        }

        private void btn_Click(object sender, EventArgs e)
        {
            Button btn = sender as Button;
            if (btn == btnOtherColor)// Other colors
            {
                this.Deactivate -= new EventHandler(ColorPicker_Deactivate);// Cancel disappear due to loss of focus
                ColorDialog colorDialog = new ColorDialog();
                if (colorDialog.ShowDialog() == DialogResult.OK)
                {
                    selectedColor = colorDialog.Color;
                }
                colorDialog.Dispose();
                SelectedColorChanged(selectedColor, EventArgs.Empty);
            }
            else if (btn == btnScreenColor)// Screen color
            {
                this.Deactivate -= new EventHandler(ColorPicker_Deactivate);// Cancel disappear due to loss of focus
                GetScreenColorForm gscf = new GetScreenColorForm();
                if (gscf.ShowDialog() == DialogResult.OK)
                {
                    selectedColor = gscf.SelectColor;
                }
                gscf.Dispose();
                SelectedColorChanged(selectedColor, EventArgs.Empty);
            }
            else if (btn != null)// Color Blocks
            {
                selectedColor = btn.BackColor;
                SelectedColorChanged(selectedColor, EventArgs.Empty);
            }
        }

        /// <summary>
        /// The form loses focus, causing it to disappear
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ColorPicker_Deactivate(object sender, EventArgs e)
        {
            SelectedColorChanged(selectedColor, EventArgs.Empty);
        }
    }
}