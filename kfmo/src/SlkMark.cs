using System.Text;

namespace KFMO;

sealed partial class SlkConv
{
    static readonly Dictionary<string, string[]> MustUse = new()
    {
        ["ability"] = new[] { "Avul", "Adda", "Amnz", "Aalr", "Aatk", "ANbu", "AHbu", "AObu", "AEbu", "AUbu", "AGbu", "Abdt", "Argd", "AHer", "Arev", "ARal", "ACsp", "Sloa", "Aetl", "Amov", "Afir", "Afih", "Afio", "Afin", "Afiu" },
        ["buff"] = new[] { "BPSE", "BSTN", "Btlf", "Bdet", "Bvul", "Bspe", "Bfro", "Bsha", "Btrv", "Xbdt", "Xbli", "Xdis", "Xfhs", "Xfhm", "Xfhl", "Xfos", "Xfom", "Xfol", "Xfns", "Xfnm", "Xfnl", "Xfus", "Xfum", "Xful" },
    };

    static readonly Dictionary<string, (string Id, string Type)> MustMark = new()
    {
        ["Asac"] = ("ushd", "unit"), ["Alam"] = ("ushd", "unit"), ["Aspa"] = ("Bspa", "buff"), ["Amil"] = ("Bmil", "buff"),
        ["AHav"] = ("BHav", "buff"), ["Aphx"] = ("Bphx", "buff"), ["Apxf"] = ("Bpxf", "buff"), ["Auns"] = ("Buns", "buff"),
        ["Asta"] = ("Bstt", "buff"), ["Achd"] = ("Bchd", "buff"), ["Aoar"] = ("Boar", "buff"), ["Aarm"] = ("Barm", "buff"),
    };

    static readonly Dictionary<string, string[]> ExtraFunc = new()
    {
        ["MeleeStartingUnitsHuman"] = new[] { "hpea", "Hamg", "Hpal", "Hblm", "Hmkg", "htow", "Amic", "stwp" },
        ["MeleeStartingUnitsOrc"] = new[] { "opeo", "Obla", "Ofar", "ogre", "Otch", "Oshd", "stwp" },
        ["MeleeStartingUnitsUndead"] = new[] { "Udre", "Udea", "Ucrl", "uaco", "unpl", "ugho", "Ulic", "stwp" },
        ["MeleeStartingUnitsNightElf"] = new[] { "etol", "Edem", "ewsp", "Ewar", "Emoo", "Ekee", "stwp" },
        ["MeleeStartingUnitsUnknownRace"] = new[] { "nshe" },
        ["MeleeStartingUnitsForPlayer"] = new[] { "hpea", "Hamg", "Hpal", "Hblm", "Hmkg", "htow", "Amic", "opeo", "Obla", "Ofar", "ogre", "Otch", "Oshd", "Udre", "Udea", "Ucrl", "uaco", "unpl", "ugho", "Ulic", "etol", "Edem", "ewsp", "Ewar", "Emoo", "Ekee", "stwp" },
        ["MeleeStartingUnits"] = new[] { "hpea", "Hamg", "Hpal", "Hblm", "Hmkg", "htow", "Amic", "opeo", "Obla", "Ofar", "ogre", "Otch", "Oshd", "Udre", "Udea", "Ucrl", "uaco", "unpl", "ugho", "Ulic", "etol", "Edem", "ewsp", "Ewar", "Emoo", "Ekee", "nshe", "stwp" },
        ["MeleeGrantItemsToHero"] = new[] { "stwp" }, ["MeleeGrantItemsToTrainedHero"] = new[] { "stwp" }, ["MeleeGrantItemsToHiredHero"] = new[] { "stwp" },
        ["MeleeGrantHeroItems"] = new[] { "stwp" }, ["MeleeRandomHeroLoc"] = new[] { "stwp" },
        ["ChangeElevatorWallBlocker"] = new[] { "DTep" }, ["NearbyElevatorExistsEnum"] = new[] { "DTrx", "DTrf" },
        ["NearbyElevatorExists"] = new[] { "DTrx", "DTrf" }, ["ChangeElevatorWalls"] = new[] { "DTep", "DTrx", "DTrf" },
    };

    static readonly Dictionary<string, string> NeedMark = new()
    {
        ["ChooseRandomCreep"] = "creeps", ["ChooseRandomCreepBJ"] = "creeps", ["ChooseRandomNPBuilding"] = "building", ["ChooseRandomNPBuildingBJ"] = "building",
        ["ChooseRandomItem"] = "item", ["ChooseRandomItemBJ"] = "item", ["ChooseRandomItemEx"] = "item", ["ChooseRandomItemExBJ"] = "item",
        ["UpdateEachStockBuildingEnum"] = "item", ["UpdateEachStockBuilding"] = "marketplace", ["PerformStockUpdates"] = "marketplace",
        ["StartStockUpdates"] = "marketplace", ["InitNeutralBuildings"] = "marketplace", ["InitBlizzard"] = "marketplace",
    };

    static Dictionary<string, Dictionary<string, string>>? search;
    static Dictionary<string, Dictionary<string, string>> Search => search ??= LoadSearch();

    static Dictionary<string, Dictionary<string, string>> LoadSearch()
    {
        var res = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        Dictionary<string, string>? cur = null;
        foreach (var raw in MetaDb.Res("search.ini").Split('\n'))
        {
            var line = raw.TrimEnd('\r').Trim();
            if (line.Length == 0) continue;
            if (line.StartsWith('[')) { cur = new(StringComparer.Ordinal); res[line[1..^1]] = cur; continue; }
            int eq = line.IndexOf('=');
            if (eq > 0 && cur != null) cur[line[..eq].Trim()] = line[(eq + 1)..].Trim();
        }
        return res;
    }

    Dictionary<string, List<string>> buffMap = new(StringComparer.Ordinal);
    readonly HashSet<WObj> markChild = new();
    public readonly List<string> NotFound = new();

    void MarkUsed(Func<string, byte[]?> get)
    {
        buffMap = new(StringComparer.Ordinal);
        foreach (var id in slk["buff"].Keys)
        {
            var li = id.ToLowerInvariant();
            if (!buffMap.TryGetValue(li, out var l)) buffMap[li] = l = new List<string>();
            l.Add(id);
        }
        foreach (var (type, list) in MustUse)
            foreach (var name in list)
                if (!MarkKnown(type, name, false)) NotFound.Add(name);

        var j = get("war3map.j") ?? get(@"scripts\war3map.j");
        var flags = new HashSet<string>();
        if (j != null)
        {
            var ids = SearchJass(j, flags);
            foreach (var id in ids) MarkAll(id);
            if (flags.Contains("building") || flags.Contains("creeps"))
            {
                string tile = MainGround(get("war3map.w3i")) ?? "*";
                foreach (var u in slk["unit"].Values)
                {
                    if (!(u.F.GetValueOrDefault("race") is string race && race == "creeps")) continue;
                    if (u.F.GetValueOrDefault("tilesets") is not string ts || !(ts == "*" || ts.Contains(tile))) continue;
                    if (flags.Contains("building") && LV.Eq(u.F.GetValueOrDefault("isbldg"), 1L) && LV.Eq(u.F.GetValueOrDefault("nbrandom"), 1L))
                        MarkKnown("unit", u.Id, false);
                    if (flags.Contains("creeps") && LV.Eq(u.F.GetValueOrDefault("isbldg"), 0L) && LV.Eq(u.F.GetValueOrDefault("special"), 0L))
                        MarkKnown("unit", u.Id, false);
                }
            }
            if (flags.Contains("item"))
                foreach (var it in slk["item"].Values)
                    if (LV.Eq(it.F.GetValueOrDefault("pickrandom"), 1L)) MarkKnown("item", it.Id, false);
        }
        var doo = get("war3map.doo");
        if (doo != null && SearchDoo(doo) is var (dest, dood) && dest != null)
        {
            foreach (var id in dest) if (!MarkKnown("destructable", id, false)) MarkKnown("doodad", id, false);
            foreach (var id in dood!) MarkKnown("doodad", id, false);
        }
        if (flags.Contains("marketplace") && !flags.Contains("item"))
            foreach (var u in slk["unit"].Values)
                if (u.Mark && u.Name == "marketplace")
                {
                    foreach (var it in slk["item"].Values)
                        if (LV.Eq(it.F.GetValueOrDefault("pickrandom"), 1L) && LV.Eq(it.F.GetValueOrDefault("sellable"), 1L))
                            MarkKnown("item", it.Id, false);
                    break;
                }
    }

    void MarkAll(string name)
    {
        foreach (var t in new[] { "ability", "unit", "buff", "item", "destructable", "doodad", "upgrade" }) MarkKnown(t, name, false);
    }

    bool MarkKnown(string type, string name, bool nosearch)
    {
        if (type == "buff")
        {
            if (!buffMap.TryGetValue(name.ToLowerInvariant(), out var m)) return false;
            foreach (var n in m) MarkKnown2("buff", n, nosearch);
            return true;
        }
        if (type == "upgrade,unit")
            return MarkKnown2("unit", name, true) || MarkKnown2("upgrade", name, true);
        return MarkKnown2(type, name, nosearch);
    }

    bool MarkKnown2(string type, string name, bool nosearch)
    {
        if (!slk.TryGetValue(type, out var tbl) || !tbl.TryGetValue(name, out var o))
            return txtLeft.ContainsKey(name.ToLowerInvariant());
        o.Mark = true;
        if (!nosearch && markChild.Add(o))
        {
            MarkList(o, Search.GetValueOrDefault(type));
            if (o.Code != null)
            {
                MarkList(o, Search.GetValueOrDefault(o.Code));
                if (MustMark.TryGetValue(o.Code, out var mm) && !MarkKnown(mm.Type, mm.Id, false)) NotFound.Add(mm.Id);
            }
        }
        return true;
    }

    void MarkList(WObj o, Dictionary<string, string>? list)
    {
        if (list == null) return;
        foreach (var (key, type) in list)
        {
            bool nosearch = key == "upgrades";
            var value = o.F.GetValueOrDefault(key);
            if (value == null) continue;
            if (value is LTable t)
            {
                for (int i = 1; t.TryGetValue(i, out var v) && v != null; i++) MarkValue(type, v, nosearch);
            }
            else MarkValue(type, value, nosearch);
        }
    }

    void MarkValue(string type, object value, bool nosearch)
    {
        if (value is not string s) return;
        foreach (var name in s.Split(',', StringSplitOptions.RemoveEmptyEntries))
            if (!MarkKnown(type, name, nosearch)) NotFound.Add(name);
    }

    static HashSet<string> SearchJass(byte[] buf, HashSet<string> flags)
    {
        const long Min = 0x41303030;
        var ids = new HashSet<string>(StringComparer.Ordinal);
        var s = Encoding.Latin1.GetString(buf);
        int i = 0, n = s.Length;
        void AddInt(string text)
        {
            if (LV.WToNumber(text) is long v && v >= Min)
                ids.Add(Encoding.Latin1.GetString(new[] { (byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8), (byte)v }));
        }
        static bool Hex(char c) => char.IsAsciiHexDigit(c);
        while (i < n)
        {
            char c = s[i];
            if (c == '/' && i + 1 < n && s[i + 1] == '/') { while (i < n && s[i] != '\n' && s[i] != '\r') i++; continue; }
            if (c == '"')
            {
                i++;
                while (i < n && s[i] != '"') { if (s[i] == '\\') i++; i++; }
                i++;
                continue;
            }
            if (c == '\'')
            {
                int st = i + 1, q = st, cnt = 0;
                while (q < n && s[q] != '\'' && cnt < 4) { if (s[q] == '\\') q++; q++; cnt++; }
                if (q < n && s[q] == '\'' && cnt >= 1)
                {
                    var id = s.Substring(st, q - st);
                    if (id.Length == 4) ids.Add(id);
                    i = q + 1;
                }
                else i++;
                continue;
            }
            if (c == '.' && i + 1 < n && char.IsAsciiDigit(s[i + 1])) { i++; while (i < n && char.IsAsciiDigit(s[i])) i++; continue; }
            if (char.IsAsciiDigit(c))
            {
                int st = i;
                if (c == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X') && i + 2 < n && Hex(s[i + 2]))
                {
                    i += 2; while (i < n && Hex(s[i])) i++;
                    AddInt(s[st..i]);
                    continue;
                }
                while (i < n && char.IsAsciiDigit(s[i])) i++;
                if (i < n && s[i] == '.') { i++; while (i < n && char.IsAsciiDigit(s[i])) i++; continue; }
                var num = s[st..i];
                if (num.Length > 1 && num[0] == '0') { AddInt("0"); AddInt(num[1..]); } else AddInt(num);
                continue;
            }
            if (c == '$' && i + 1 < n && Hex(s[i + 1]))
            {
                int st = ++i; while (i < n && Hex(s[i])) i++;
                AddInt("0x" + s[st..i]);
                continue;
            }
            if (char.IsAsciiLetter(c))
            {
                int st = i; while (i < n && (char.IsAsciiLetterOrDigit(s[i]) || s[i] == '_')) i++;
                var w = s[st..i];
                if (ExtraFunc.TryGetValue(w, out var extra)) foreach (var e in extra) ids.Add(e);
                if (NeedMark.TryGetValue(w, out var f)) flags.Add(f);
                continue;
            }
            i++;
        }
        return ids;
    }

    static (List<string>? Dest, List<string>? Dood) SearchDoo(byte[] buf)
    {
        try
        {
            var r = new BinaryReader(new MemoryStream(buf));
            r.ReadInt32(); int version = r.ReadInt32(); r.ReadInt32(); int n = r.ReadInt32();
            var dest = new List<string>(); var dood = new List<string>();
            for (int i = 0; i < n; i++)
            {
                dest.Add(Encoding.Latin1.GetString(r.ReadBytes(4)));
                r.ReadBytes(4 + 28 + 2 + 4);
                if (version >= 8)
                {
                    int sets = r.ReadInt32();
                    for (int k = 0; k < sets; k++) { int items = r.ReadInt32(); r.ReadBytes(8 * items); }
                    r.ReadInt32();
                }
            }
            r.ReadInt32(); int m = r.ReadInt32();
            for (int i = 0; i < m; i++) { dood.Add(Encoding.Latin1.GetString(r.ReadBytes(4))); r.ReadBytes(12); }
            return (dest, dood);
        }
        catch (EndOfStreamException) { return (null, null); }
    }

    public static int? W3iFlags(byte[]? w3i)
    {
        if (w3i == null) return null;
        try
        {
            var r = new BinaryReader(new MemoryStream(w3i));
            int ver = r.ReadInt32(); r.ReadInt32(); if (ver >= 25) r.ReadInt32();
            for (int k = 0; k < 4; k++) while (r.ReadByte() != 0) { }
            r.ReadBytes(8 * 4 + 4 * 4 + 4 + 4);
            return r.ReadInt32();
        }
        catch { return null; }
    }

    static string? MainGround(byte[]? w3i)
    {
        if (w3i == null) return null;
        try
        {
            var r = new BinaryReader(new MemoryStream(w3i));
            int ver = r.ReadInt32(); r.ReadInt32(); if (ver >= 25) r.ReadInt32();
            for (int k = 0; k < 4; k++) while (r.ReadByte() != 0) { }
            r.ReadBytes(8 * 4 + 4 * 4 + 4 + 4 + 4);
            return ((char)r.ReadByte()).ToString();
        }
        catch { return null; }
    }
}
