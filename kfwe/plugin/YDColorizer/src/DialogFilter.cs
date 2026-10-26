using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;

namespace YDColorizer
{
    static class DialogFilter
    {
        private delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr hWndParent, EnumProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

        [DllImport("user32.dll")]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        private static extern IntPtr GetParent(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool IsWindow(IntPtr hWnd);

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern int GetWindowTextLength(IntPtr hWnd);

        private const int GWL_STYLE = -16;
        private const int ES_READONLY = 0x0800;
        private const int ES_NUMBER = 0x2000;

        private static readonly Regex Number = new Regex(@"^\s*[-+]?\d+([.,]\d+)?\s*$");

        private static IntPtr lastRejected = IntPtr.Zero;

        public static bool IsRejected(IntPtr hWnd)
        {
            return hWnd == lastRejected && IsWindow(hWnd);
        }

        public static bool IsTextValueDialog(IntPtr hWnd)
        {
            int edits = 0, buttons = 0, other = 0;
            IntPtr edit = IntPtr.Zero;
            EnumChildWindows(hWnd, (child, lParam) =>
            {
                if (GetParent(child) != hWnd)
                {
                    return true;
                }
                StringBuilder cls = new StringBuilder(64);
                GetClassName(child, cls, cls.Capacity);
                switch (cls.ToString())
                {
                    case "Edit": edits++; edit = child; break;
                    case "Button": buttons++; break;
                    case "Static": break;
                    default: other++; break;
                }
                return true;
            }, IntPtr.Zero);

            bool ok = edits == 1 && buttons >= 2 && other == 0;
            if (ok && (GetWindowLong(edit, GWL_STYLE) & (ES_READONLY | ES_NUMBER)) != 0)
            {
                ok = false;
            }
            if (ok)
            {
                int length = GetWindowTextLength(edit);
                StringBuilder text = new StringBuilder(length + 1);
                GetWindowText(edit, text, text.Capacity);
                if (Number.IsMatch(text.ToString()))
                {
                    ok = false;
                }
            }
            if (!ok)
            {
                lastRejected = hWnd;
            }
            return ok;
        }
    }
}
