using System;
using System.Collections.Generic;
using System.Text;
using System.Reflection;
using System.IO;
using System.Windows.Forms;
using System.Drawing;

namespace YDColorizer
{
    public class Config
    {
        /// <summary>
        /// Get the directory where the assembly is located
        /// </summary>
        /// <returns>The full path to the assembly</returns>
        public static string GetThisMoudlePath()
        {
            string str = Assembly.GetExecutingAssembly().CodeBase;// Get the full path of the assembly
            int start = 8;// Remove file:///
            int end = str.LastIndexOf('/');// Remove the file name xxx.dll
            return str.Substring(start, end - start);
        }

        /// <summary>
        /// Configuration file name
        /// </summary>
        private const string ConfigName = "YDColorizerConfig.ini";

        /// <summary>
        /// Get the configuration file path
        /// </summary>
        /// <returns>The full path to the configuration file</returns>
        public static string GetConfigPath()
        {
            return GetThisMoudlePath() + "\\" + ConfigName;
        }

        /// <summary>
        /// Get whether the configuration file exists
        /// </summary>
        /// <returns></returns>
        public static bool IsConfigExists()
        {
            return File.Exists(GetConfigPath());
        }

        /// <summary>
        /// Save window size to configuration file
        /// </summary>
        /// <param name="form"></param>
        public static void SaveDialogSize(Form form)
        {
            if (IsConfigExists() == true)
            {
                if (WinApi.Ini.GetInt(GetConfigPath(), "Application", "RememberSize") != 0)
                {
                    WinApi.Ini.SetInt(GetConfigPath(), "Application", "Width", form.Width);
                    WinApi.Ini.SetInt(GetConfigPath(), "Application", "Height", form.Height);
                }
            }
        }

        /// <summary>
        /// 从配置文件加载窗口大小
        /// </summary>
        /// <param name="form"></param>
        /// <returns>成功则返回true</returns>
        public static bool LoadDialogSize(Form form)
        {
            if (IsConfigExists() == true)// 配置文件存在
            {
                if (WinApi.Ini.GetInt(GetConfigPath(), "Application", "RememberSize") != 0)// 配置保存窗口大小
                {
                    int width = WinApi.Ini.GetInt(GetConfigPath(), "Application", "Width");
                    int height = WinApi.Ini.GetInt(GetConfigPath(), "Application", "Height");
                    if (width > 0 && height > 0)// Correct range
                    {
                        form.Width = width;
                        form.Height = height;
                        return true;
                    }
                    else// Range Error
                    {
                        return false;
                    }
                }
                else// Configuration does not save window size
                {
                    return false;
                }
            }
            else// The configuration file does not exist
            {
                return false;
            }
        }

        /// <summary>
        /// Load button colors from a configuration file
        /// </summary>
        /// <param name="btn">Button</param>
        public static void LoadColor(Button btn)
        {
            if (File.Exists(Config.GetConfigPath()) == true)// Configuration file exists
            {
                string[] color = WinApi.Ini.GetString(Config.GetConfigPath(), "UserCustomColor", btn.Name).Split(',');// Get the RGB value in the configuration file
                if (color.Length == 3)// Correct format
                {
                    byte r, g, b;
                    if (byte.TryParse(color[0], out r) == true && byte.TryParse(color[1], out g) == true && byte.TryParse(color[2], out b) == true)// The range is correct and can be converted
                    {
                        btn.BackColor = Color.FromArgb(r, g, b);
                    }
                    else// Range error or text error
                    {
                        btn.BackColor = Color.Black;
                    }
                }
                else// Format Error
                {
                    btn.BackColor = Color.Black;
                }
            }
            else// The configuration file does not exist
            {
                btn.BackColor = Color.Black;
            }
        }

        /// <summary>
        /// Save button colors to configuration file
        /// </summary>
        /// <param name="btn">Button</param>
        public static void SaveColor(Button btn)
        {
            if (File.Exists(Config.GetConfigPath()) == true)// Configuration file exists
            {
                Color color = btn.BackColor;
                string str = color.R + "," + color.G + "," + color.B;
                WinApi.Ini.SetString(Config.GetConfigPath(), "UserCustomColor", btn.Name, str);
            }
        }

        public static void CreateDefault()
        {
            using (StreamWriter sw = new StreamWriter(GetConfigPath(), false, Encoding.Default))
            {
                sw.WriteLine("[YDColorizerConfig]");
                sw.WriteLine("; This is the configuration file of YDColorizer. Please do not modify it at will.");
                sw.WriteLine("[Application]");
                sw.WriteLine("RememberSize=0");
                sw.WriteLine("; Remember the last window size, 0->don't remember, non-0->remember");
                sw.WriteLine("Width=400");
                sw.WriteLine("; Last window width");
                sw.WriteLine("Height=300");
                sw.WriteLine("; Last window height");
                sw.WriteLine("[UserCustomColor]");
                sw.WriteLine("btn1stColor=255,0,0");
                sw.WriteLine(";RGB value of the main color");
                sw.WriteLine("btn2ndColor=0,255,0");
                sw.WriteLine("; The RGB value of the first candidate color");
                sw.WriteLine("btn3rdColor=0,0,255");
                sw.WriteLine("; The RGB value of the second candidate color");
                sw.WriteLine("btn4thColor=255,255,0");
                sw.WriteLine("; The RGB value of the third candidate color");
            }
        }
    }
}
