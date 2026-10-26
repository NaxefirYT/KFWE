using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.IO;
using System.Text;

namespace YDColorizer
{
    class GlobalException
    {
        public static void CatchException(Exception e, string default_msg)
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("***************************************************************");
            sb.AppendLine("    There is a serious error in your color plugin. Please take a screenshot and send it to www.ydwe.net。   ");
            sb.AppendLine("***************************************************************");
            if (e != null)
            {                
                sb.AppendLine("【Exception Type】：" + e.GetType().Name);
                sb.AppendLine("【Exception information】：" + e.Message);
                sb.AppendLine("【Call Stack】：" + e.StackTrace);
            }
            else
            {
                sb.AppendLine("【Unhandled Exception】：" + default_msg);
            }
            sb.AppendLine("***************************************************************");
            MessageBox.Show(sb.ToString(), "YDColorizer");
        }

        static void CatchThreadException(object sender, System.Threading.ThreadExceptionEventArgs e)
        {
            CatchException(e.Exception as Exception, e.ToString());
        }

        static void CatchUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            CatchException(e.ExceptionObject as Exception, e.ToString());
        }
 
        public static void Initialize()
        {
            try
            { 
                Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
                Application.ThreadException += new System.Threading.ThreadExceptionEventHandler(CatchThreadException);
                AppDomain.CurrentDomain.UnhandledException += new UnhandledExceptionEventHandler(CatchUnhandledException);
            }
            catch (Exception e)
            {
                CatchException(e, e.ToString());
            }
        }
    }
}
