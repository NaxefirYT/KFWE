using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace YDColorizer
{
    class AboutProcess
    {
        [DllImport("user32.dll", EntryPoint = "GetWindowThreadProcessId")]
        private static extern int GetWindowThreadProcessId(IntPtr hwnd, out int ID);

        /// <summary>
        /// Get the process ID of the window
        /// </summary>
        /// <param name="hwnd">Window handle</param>
        /// <returns>Process number</returns>
        public static int GetProcessId(IntPtr hwnd)
        {
            int processId;
            GetWindowThreadProcessId(hwnd, out processId);
            return processId;
        }
    }
}
