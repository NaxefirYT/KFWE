using System.Text;

namespace KFMO;

static class JassMin
{
    enum K { Word, Num, Str, Raw, Op, Nl }
    readonly record struct Tok(K Kind, string Text);

    static readonly HashSet<string> Keywords = new(StringComparer.Ordinal)
    {
        "if", "then", "else", "elseif", "endif", "loop", "endloop", "exitwhen", "return", "set", "call", "local", "function",
        "endfunction", "takes", "returns", "nothing", "globals", "endglobals", "constant", "native", "type", "extends", "array",
        "and", "or", "not", "true", "false", "null", "code", "handle", "integer", "real", "boolean", "string", "debug",
    };

    public sealed class Result { public int Renamed, Functions, Globals, Locals; }

    static List<Tok> Lex(string s)
    {
        var t = new List<Tok>();
        int i = 0, n = s.Length;
        while (i < n)
        {
            char c = s[i];
            if (c == '\n') { if (t.Count > 0 && t[^1].Kind != K.Nl) t.Add(new Tok(K.Nl, "\n")); i++; continue; }
            if (c == ' ' || c == '\t' || c == '\r' || c == '﻿') { i++; continue; }
            if (c == '/' && i + 1 < n && s[i + 1] == '/') { while (i < n && s[i] != '\n') i++; continue; }
            int st = i;
            if (c == '"' || c == '\'')
            {
                i++;
                while (i < n && s[i] != c) { if (s[i] == '\\') i++; i++; }
                i++;
                t.Add(new Tok(c == '"' ? K.Str : K.Raw, s[st..Math.Min(i, n)]));
                continue;
            }
            if (char.IsAsciiDigit(c) || c == '$' || (c == '.' && i + 1 < n && char.IsAsciiDigit(s[i + 1])))
            {
                i++;
                while (i < n && (char.IsAsciiLetterOrDigit(s[i]) || s[i] == '.')) i++;
                t.Add(new Tok(K.Num, s[st..i]));
                continue;
            }
            if (char.IsAsciiLetter(c) || c == '_')
            {
                while (i < n && (char.IsAsciiLetterOrDigit(s[i]) || s[i] == '_')) i++;
                t.Add(new Tok(K.Word, s[st..i]));
                continue;
            }
            if (i + 1 < n && (s[i + 1] == '=') && (c == '=' || c == '!' || c == '<' || c == '>')) { t.Add(new Tok(K.Op, s.Substring(i, 2))); i += 2; continue; }
            t.Add(new Tok(K.Op, c.ToString()));
            i++;
        }
        return t;
    }

    public static IEnumerable<string> Words(string s) => Lex(s).Where(x => x.Kind == K.Word).Select(x => x.Text);

    public static string? Minify(string src, IEnumerable<string> reservedExtra, out Result res)
    {
        res = new Result();
        var toks = Lex(src);
        var reserved = new HashSet<string>(Keywords, StringComparer.Ordinal) { "main", "config" };
        foreach (var w in reservedExtra) reserved.Add(w);
        foreach (var x in toks)
            if (x.Kind == K.Str && x.Text.Length >= 2) reserved.Add(x.Text[1..^1]);

        var lines = new List<List<int>>();
        var cur = new List<int>();
        for (int i = 0; i < toks.Count; i++)
        {
            if (toks[i].Kind == K.Nl) { if (cur.Count > 0) lines.Add(cur); cur = new List<int>(); }
            else cur.Add(i);
        }
        if (cur.Count > 0) lines.Add(cur);

        var globals = new HashSet<string>(StringComparer.Ordinal);
        var functions = new HashSet<string>(StringComparer.Ordinal);
        var funcOfTok = new int[toks.Count];
        Array.Fill(funcOfTok, -1);
        var funcLocals = new List<HashSet<string>>();
        bool inGlobals = false; int fn = -1;
        foreach (var L in lines)
        {
            string W(int k) => k < L.Count ? toks[L[k]].Text : "";
            int p = 0;
            if (W(0) == "globals") { inGlobals = true; continue; }
            if (W(0) == "endglobals") { inGlobals = false; continue; }
            if (inGlobals)
            {
                if (W(p) == "constant") p++;
                p++;
                if (W(p) == "array") p++;
                if (p < L.Count && toks[L[p]].Kind == K.Word) globals.Add(W(p));
                continue;
            }
            if (W(0) == "native" || (W(0) == "constant" && W(1) == "native")) { reserved.Add(W(0) == "native" ? W(1) : W(2)); continue; }
            if (W(0) == "type") { reserved.Add(W(1)); continue; }
            if (W(0) == "function" || (W(0) == "constant" && W(1) == "function"))
            {
                p = W(0) == "function" ? 1 : 2;
                functions.Add(W(p));
                fn = funcLocals.Count;
                var loc = new HashSet<string>(StringComparer.Ordinal);
                funcLocals.Add(loc);
                int t = L.FindIndex(k => toks[k].Text == "takes");
                if (t >= 0 && W(t + 1) != "nothing")
                    for (int k = t + 1; k < L.Count && W(k) != "returns"; k++)
                        if (W(k) != "," && (k + 1 >= L.Count || W(k + 1) == "," || W(k + 1) == "returns")) loc.Add(W(k));
                foreach (var k in L) funcOfTok[k] = fn;
                continue;
            }
            if (fn >= 0)
            {
                foreach (var k in L) funcOfTok[k] = fn;
                if (W(0) == "local")
                {
                    p = 2;
                    if (W(p) == "array") p++;
                    funcLocals[fn].Add(W(p));
                }
                if (W(0) == "endfunction") fn = -1;
            }
        }

        var gCount = new Dictionary<string, int>(StringComparer.Ordinal);
        var lCount = funcLocals.Select(_ => new Dictionary<string, int>(StringComparer.Ordinal)).ToList();
        for (int i = 0; i < toks.Count; i++)
        {
            if (toks[i].Kind != K.Word) continue;
            var w = toks[i].Text;
            int f = funcOfTok[i];
            if (f >= 0 && funcLocals[f].Contains(w)) { if (!reserved.Contains(w)) lCount[f][w] = lCount[f].GetValueOrDefault(w) + 1; }
            else if ((globals.Contains(w) || functions.Contains(w)) && !reserved.Contains(w)) gCount[w] = gCount.GetValueOrDefault(w) + 1;
        }
        var slotWeight = new List<long>();
        var localSlot = new List<Dictionary<string, int>>();
        foreach (var lc in lCount)
        {
            var order = lc.OrderByDescending(kv => kv.Value).ThenBy(kv => kv.Key, StringComparer.Ordinal).ToList();
            var m = new Dictionary<string, int>(StringComparer.Ordinal);
            for (int r = 0; r < order.Count; r++)
            {
                if (slotWeight.Count <= r) slotWeight.Add(0);
                slotWeight[r] += order[r].Value;
                m[order[r].Key] = r;
            }
            localSlot.Add(m);
        }
        var ents = gCount.Select(kv => (Key: "g:" + kv.Key, W: (long)kv.Value))
            .Concat(slotWeight.Select((w, r) => (Key: "l:" + r, W: w)))
            .OrderByDescending(e => e.W).ThenBy(e => e.Key, StringComparer.Ordinal).ToList();
        var names = NameGen().Where(nm => !reserved.Contains(nm) && !globals.Contains(nm) && !functions.Contains(nm)).GetEnumerator();
        var gNew = new Dictionary<string, string>(StringComparer.Ordinal);
        var slotNew = new Dictionary<int, string>();
        foreach (var e in ents)
        {
            names.MoveNext();
            if (e.Key[0] == 'g') gNew[e.Key[2..]] = names.Current; else slotNew[int.Parse(e.Key[2..])] = names.Current;
        }
        res.Functions = gCount.Keys.Count(functions.Contains);
        res.Globals = gCount.Count - res.Functions;
        res.Locals = lCount.Sum(x => x.Count);
        res.Renamed = res.Functions + res.Globals + res.Locals;

        var sb = new StringBuilder(src.Length / 2);
        bool lineStart = true; char prevLast = '\0';
        for (int i = 0; i < toks.Count; i++)
        {
            var x = toks[i];
            if (x.Kind == K.Nl) { if (!lineStart) { sb.Append('\n'); lineStart = true; } continue; }
            string text = x.Text;
            if (x.Kind == K.Word)
            {
                int f = funcOfTok[i];
                if (f >= 0 && localSlot[f].TryGetValue(text, out int slot)) text = slotNew[slot];
                else if (gNew.TryGetValue(text, out var g) && !(f >= 0 && funcLocals[f].Contains(text))) text = g;
            }
            else if (x.Kind == K.Num) text = ShortNum(text);
            if (!lineStart && Wordy(prevLast) && Wordy(text[0])) sb.Append(' ');
            sb.Append(text);
            prevLast = text[^1];
            lineStart = false;
        }
        if (!lineStart) sb.Append('\n');
        return sb.ToString();
    }

    static bool Wordy(char c) => char.IsAsciiLetterOrDigit(c) || c == '_' || c == '.' || c == '$' || c == '\'' || c == '"';

    static string ShortNum(string s)
    {
        if (!s.Contains('.') || s.StartsWith("0x", StringComparison.OrdinalIgnoreCase) || s.StartsWith('$')) return s;
        foreach (char c in s) if (!char.IsAsciiDigit(c) && c != '.') return s;
        var t = s.TrimEnd('0');
        int dot = t.IndexOf('.');
        var ip = t[..dot].TrimStart('0');
        var fp = t[(dot + 1)..];
        if (ip.Length == 0 && fp.Length == 0) return "0.";
        return ip + "." + fp;
    }

    static IEnumerable<string> NameGen()
    {
        const string first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const string rest = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        foreach (var c in first) yield return c.ToString();
        for (int len = 2; ; len++)
        {
            var idx = new int[len];
            while (true)
            {
                var sb = new StringBuilder(len);
                sb.Append(first[idx[0]]);
                for (int k = 1; k < len; k++) sb.Append(rest[idx[k]]);
                yield return sb.ToString();
                int pos = len - 1;
                while (pos >= 0)
                {
                    idx[pos]++;
                    if (idx[pos] < (pos == 0 ? first.Length : rest.Length)) break;
                    idx[pos] = 0; pos--;
                }
                if (pos < 0) break;
            }
        }
    }
}
