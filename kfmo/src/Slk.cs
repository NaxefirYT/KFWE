using System.Globalization;
using System.Text;

namespace KFMO;

sealed class SlkTable
{
    public List<string> Columns = new();
    public Dictionary<string, int> ColumnIndex = new(StringComparer.OrdinalIgnoreCase);
    public List<string> RowOrder = new();
    public Dictionary<string, Dictionary<string, string>> Rows = new(StringComparer.Ordinal);

    public static SlkTable Parse(byte[] data)
    {
        var t = new SlkTable();
        var cells = new Dictionary<(int x, int y), string>();
        int x = 1, y = 1, maxX = 0, maxY = 0;
        var text = Encoding.UTF8.GetString(data);
        foreach (var rawLine in text.Split('\n'))
        {
            var line = rawLine.TrimEnd('\r');
            if (line.Length < 2 || line[0] != 'C' || line[1] != ';') continue;
            string? value = null;
            foreach (var part in SplitFields(line.Substring(2)))
            {
                if (part.Length == 0) continue;
                switch (part[0])
                {
                    case 'X': int.TryParse(part.AsSpan(1), out x); break;
                    case 'Y': int.TryParse(part.AsSpan(1), out y); break;
                    case 'K': value = Unquote(part.Substring(1)); break;
                }
            }
            if (value != null)
            {
                cells[(x, y)] = value;
                maxX = Math.Max(maxX, x); maxY = Math.Max(maxY, y);
            }
        }
        for (int c = 1; c <= maxX; c++)
        {
            string name = cells.TryGetValue((c, 1), out var v) ? v : "";
            t.Columns.Add(name);
            if (name.Length > 0 && !t.ColumnIndex.ContainsKey(name)) t.ColumnIndex[name] = c - 1;
        }
        for (int r = 2; r <= maxY; r++)
        {
            if (!cells.TryGetValue((1, r), out var id) || id.Length == 0) continue;
            var row = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            for (int c = 2; c <= maxX; c++)
                if (cells.TryGetValue((c, r), out var val) && t.Columns[c - 1].Length > 0)
                    row[t.Columns[c - 1]] = val;
            if (!t.Rows.ContainsKey(id)) t.RowOrder.Add(id);
            t.Rows[id] = row;
        }
        return t;
    }

    static IEnumerable<string> SplitFields(string s)
    {
        var sb = new StringBuilder();
        bool q = false;
        foreach (char ch in s)
        {
            if (ch == '"') q = !q;
            if (ch == ';' && !q) { yield return sb.ToString(); sb.Clear(); continue; }
            sb.Append(ch);
        }
        yield return sb.ToString();
    }

    static string Unquote(string v) => v.Length >= 2 && v[0] == '"' && v[^1] == '"' ? v.Substring(1, v.Length - 2) : v;

    public string IdColumn => Columns.Count > 0 ? Columns[0] : "";

    public byte[] Write()
    {
        var sb = new StringBuilder();
        sb.Append("ID;PWXL;N;E\r\n");
        sb.Append("B;X").Append(Columns.Count).Append(";Y").Append(RowOrder.Count + 1).Append(";D0\r\n");
        for (int c = 0; c < Columns.Count; c++)
            sb.Append("C;X").Append(c + 1).Append(c == 0 ? ";Y1" : "").Append(";K\"").Append(Columns[c]).Append("\"\r\n");
        int y = 2;
        foreach (var id in RowOrder)
        {
            var row = Rows[id];
            sb.Append("C;X1;Y").Append(y).Append(";K\"").Append(id).Append("\"\r\n");
            for (int c = 1; c < Columns.Count; c++)
            {
                if (!row.TryGetValue(Columns[c], out var v) || v.Length == 0) continue;
                sb.Append("C;X").Append(c + 1).Append(";K").Append(Format(v)).Append("\r\n");
            }
            y++;
        }
        sb.Append("E\r\n");
        return Encoding.UTF8.GetBytes(sb.ToString());
    }

    static string Format(string v)
    {
        if (double.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out double d) && !v.Contains('"')
            && v.Trim() == v && !v.StartsWith("+") && !(v.Length > 1 && v[0] == '0' && v[1] != '.'))
        {
            if (d == Math.Floor(d) && Math.Abs(d) < 1e9) return ((long)d).ToString(CultureInfo.InvariantCulture);
            return d.ToString("0.#######", CultureInfo.InvariantCulture);
        }
        return "\"" + v + "\"";
    }
}
