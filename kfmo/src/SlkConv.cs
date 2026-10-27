using System.Text;
using System.Text.RegularExpressions;

namespace KFMO;

sealed class WObj
{
    public string Id = "", Type = "";
    public string? Parent, Code, Name;
    public object? MaxLevel;
    public bool IsObj, KeepObj, Slk, Mark = true;
    public Dictionary<string, object?> F = new(StringComparer.Ordinal);

    public WObj CloneShallowAttrs() => new()
    {
        Id = Id, Type = Type, Parent = Parent, Code = Code, Name = Name, MaxLevel = MaxLevel,
        IsObj = IsObj, KeepObj = KeepObj, Slk = Slk, Mark = Mark,
    };
}

sealed partial class SlkConv
{
    public static readonly string[] Types = { "ability", "buff", "unit", "item", "upgrade", "doodad", "destructable" };

    static readonly Dictionary<string, string[]> SlkFiles = new()
    {
        ["ability"] = new[] { @"units\abilitydata.slk" },
        ["destructable"] = new[] { @"units\destructabledata.slk" },
        ["doodad"] = new[] { @"doodads\doodads.slk" },
        ["buff"] = new[] { @"units\abilitybuffdata.slk" },
        ["upgrade"] = new[] { @"units\upgradedata.slk" },
        ["item"] = new[] { @"units\itemdata.slk" },
        ["unit"] = new[] { @"units\unitui.slk", @"units\unitdata.slk", @"units\unitbalance.slk", @"units\unitabilities.slk", @"units\unitweapons.slk" },
    };

    public static readonly Dictionary<string, string> ObjFile = new()
    {
        ["ability"] = "war3map.w3a", ["destructable"] = "war3map.w3b", ["doodad"] = "war3map.w3d", ["buff"] = "war3map.w3h",
        ["upgrade"] = "war3map.w3q", ["item"] = "war3map.w3t", ["unit"] = "war3map.w3u",
    };

    static readonly Dictionary<string, string> LevelKey = new() { ["ability"] = "levels", ["doodad"] = "numvar", ["upgrade"] = "maxlevel" };

    public static readonly string[] TxtFiles =
    {
        @"units\campaignunitstrings.txt", @"units\humanunitstrings.txt", @"units\neutralunitstrings.txt", @"units\nightelfunitstrings.txt",
        @"units\orcunitstrings.txt", @"units\undeadunitstrings.txt", @"units\campaignunitfunc.txt", @"units\humanunitfunc.txt",
        @"units\neutralunitfunc.txt", @"units\nightelfunitfunc.txt", @"units\orcunitfunc.txt", @"units\undeadunitfunc.txt",
        @"units\campaignabilitystrings.txt", @"units\commonabilitystrings.txt", @"units\humanabilitystrings.txt", @"units\neutralabilitystrings.txt",
        @"units\nightelfabilitystrings.txt", @"units\orcabilitystrings.txt", @"units\undeadabilitystrings.txt", @"units\itemabilitystrings.txt",
        @"units\campaignabilityfunc.txt", @"units\commonabilityfunc.txt", @"units\humanabilityfunc.txt", @"units\neutralabilityfunc.txt",
        @"units\nightelfabilityfunc.txt", @"units\orcabilityfunc.txt", @"units\undeadabilityfunc.txt", @"units\itemabilityfunc.txt",
        @"units\campaignupgradestrings.txt", @"units\neutralupgradestrings.txt", @"units\nightelfupgradestrings.txt", @"units\humanupgradestrings.txt",
        @"units\orcupgradestrings.txt", @"units\undeadupgradestrings.txt", @"units\campaignupgradefunc.txt", @"units\humanupgradefunc.txt",
        @"units\neutralupgradefunc.txt", @"units\nightelfupgradefunc.txt", @"units\orcupgradefunc.txt", @"units\undeadupgradefunc.txt",
        @"units\itemstrings.txt", @"units\itemfunc.txt",
    };

    static readonly Dictionary<string, string> TxtOut = new()
    {
        ["unit"] = @"units\campaignunitstrings.txt", ["ability"] = @"units\campaignabilitystrings.txt", ["buff"] = @"units\commonabilitystrings.txt",
        ["upgrade"] = @"units\campaignupgradestrings.txt", ["item"] = @"units\itemstrings.txt", ["txt"] = @"units\itemabilitystrings.txt",
    };

    static readonly Dictionary<string, string[]> SlkKeys = new()
    {
        [@"units\abilitydata.slk"] = S("alias,code,Area1,Area2,Area3,Area4,BuffID1,BuffID2,BuffID3,BuffID4,Cast1,Cast2,Cast3,Cast4,checkDep,Cool1,Cool2,Cool3,Cool4,Cost1,Cost2,Cost3,Cost4,DataA1,DataA2,DataA3,DataA4,DataB1,DataB2,DataB3,DataB4,DataC1,DataC2,DataC3,DataC4,DataD1,DataD2,DataD3,DataD4,DataE1,DataE2,DataE3,DataE4,DataF1,DataF2,DataF3,DataF4,DataG1,DataG2,DataG3,DataG4,DataH1,DataH2,DataH3,DataH4,DataI1,DataI2,DataI3,DataI4,Dur1,Dur2,Dur3,Dur4,EfctID1,EfctID2,EfctID3,EfctID4,HeroDur1,HeroDur2,HeroDur3,HeroDur4,levels,levelSkip,priority,reqLevel,Rng1,Rng2,Rng3,Rng4,targs1,targs2,targs3,targs4,UnitID1,UnitID2,UnitID3,UnitID4"),
        [@"units\abilitybuffdata.slk"] = S("alias"),
        [@"units\destructabledata.slk"] = S("DestructableID,armor,cliffHeight,colorB,colorG,colorR,deathSnd,fatLOS,file,fixedRot,flyH,fogRadius,fogVis,HP,lightweight,maxPitch,maxRoll,maxScale,minScale,MMBlue,MMGreen,MMRed,Name,numVar,occH,pathTex,pathTexDeath,portraitmodel,radius,selcircsize,selectable,shadow,showInMM,targType,texFile,texID,tilesetSpecific,useMMColor,walkable"),
        [@"units\itemdata.slk"] = S("itemID,abilList,armor,class,colorB,colorG,colorR,cooldownID,drop,droppable,file,goldcost,HP,ignoreCD,Level,lumbercost,morph,oldLevel,pawnable,perishable,pickRandom,powerup,prio,scale,sellable,stockMax,stockRegen,stockStart,usable,uses"),
        [@"units\upgradedata.slk"] = S("upgradeid,base1,base2,base3,base4,class,code1,code2,code3,code4,effect1,effect2,effect3,effect4,global,goldbase,goldmod,inherit,lumberbase,lumbermod,maxlevel,mod1,mod2,mod3,mod4,timebase,timemod,used"),
        [@"units\unitabilities.slk"] = S("unitAbilID,abilList,auto,heroAbilList"),
        [@"units\unitbalance.slk"] = S("unitBalanceID,AGI,AGIplus,bldtm,bountydice,bountyplus,bountysides,collision,def,defType,defUp,fmade,fused,goldcost,goldRep,HP,INT,INTplus,isbldg,level,lumberbountydice,lumberbountyplus,lumberbountysides,lumbercost,lumberRep,mana0,manaN,maxSpd,minSpd,nbrandom,nsight,preventPlace,Primary,regenHP,regenMana,regenType,reptm,repulse,repulseGroup,repulseParam,repulsePrio,requirePlace,sight,spd,stockMax,stockRegen,stockStart,STR,STRplus,tilesets,type,upgrades"),
        [@"units\unitdata.slk"] = S("unitID,buffRadius,buffType,canBuildOn,canFlee,canSleep,cargoSize,death,deathType,fatLOS,formation,isBuildOn,moveFloor,moveHeight,movetp,nameCount,orientInterp,pathTex,points,prio,propWin,race,requireWaterRadius,targType,turnRate"),
        [@"units\unitui.slk"] = S("unitUIID,name,armor,blend,blue,buildingShadow,customTeamColor,elevPts,elevRad,file,fileVerFlags,fogRad,green,hideHeroBar,hideHeroDeathMsg,hideHeroMinimap,hideOnMinimap,maxPitch,maxRoll,modelScale,nbmmIcon,occH,red,run,scale,scaleBull,selCircOnWater,selZ,shadowH,shadowOnWater,shadowW,shadowX,shadowY,teamColor,tilesetSpecific,uberSplat,unitShadow,unitSound,walk"),
        [@"units\unitweapons.slk"] = S("unitWeapID,acquire,atkType1,atkType2,backSw1,backSw2,castbsw,castpt,cool1,cool2,damageLoss1,damageLoss2,dice1,dice2,dmgplus1,dmgplus2,dmgpt1,dmgpt2,dmgUp1,dmgUp2,Farea1,Farea2,Harea1,Harea2,Hfact1,Hfact2,impactSwimZ,impactZ,launchSwimZ,launchX,launchY,launchZ,minRange,Qarea1,Qarea2,Qfact1,Qfact2,rangeN1,rangeN2,RngBuff1,RngBuff2,showUI1,showUI2,sides1,sides2,spillDist1,spillDist2,spillRadius1,spillRadius2,splashTargs1,splashTargs2,targCount1,targCount2,targs1,targs2,weapsOn,weapTp1,weapTp2,weapType1,weapType2"),
        [@"doodads\doodads.slk"] = S("doodID,file,Name,doodClass,soundLoop,selSize,defScale,minScale,maxScale,maxPitch,maxRoll,visRadius,walkable,numVar,floats,shadow,showInFog,animInFog,fixedRot,pathTex,showInMM,useMMColor,MMRed,MMGreen,MMBlue,vertR01,vertG01,vertB01,vertR02,vertG02,vertB02,vertR03,vertG03,vertB03,vertR04,vertG04,vertB04,vertR05,vertG05,vertB05,vertR06,vertG06,vertB06,vertR07,vertG07,vertB07,vertR08,vertG08,vertB08,vertR09,vertG09,vertB09,vertR10,vertG10,vertB10"),
    };

    static string[] S(string s) => s.Split(',');

    // Reforged (1.32+) берёт модели из skin-файлов, а не из столбца file в SLK: без них юниты невидимы. 1.26 их не читает.
    static readonly Dictionary<string, (string Path, string Kind, string[] Keys)> SkinOut = new()
    {
        [@"units\unitui.slk"] = (@"units\unitskin.txt", "unit", S("file,fileVerFlags,unitSound,armor,blend,scale,scaleBull,modelScale,red,green,blue,teamColor,customTeamColor,elevPts,elevRad,maxPitch,maxRoll,occH,run,walk,selZ,selCircOnWater,fogRad,unitShadow,buildingShadow,shadowW,shadowH,shadowX,shadowY,shadowOnWater,uberSplat")),
        [@"units\itemdata.slk"] = (@"units\itemskin.txt", "item", S("file,scale,colorR,colorG,colorB,armor")),
    };

    static readonly string[] YdweBuffCols = { "code", "comments", "isEffect", "version", "useInEditor", "sort", "race", "InBeta" };
    static readonly (string Id, object[] V)[] YdweBuffs =
    {
        ("Bdbl", new object[] { "YDWE", 0L, 1L, 1L, "hero", "human", 1L }), ("Bdbm", new object[] { "YDWE", 0L, 1L, 1L, "hero", "human", 1L }),
        ("BHtb", new object[] { "YDWE", 0L, 1L, 1L, "unit", "other", 1L }), ("Bsta", new object[] { "YDWE", 0L, 1L, 1L, "unit", "orc", 1L }),
        ("Bdbb", new object[] { "YDWE", 0L, 1L, 1L, "hero", "human", 1L }), ("BIpb", new object[] { "YDWE", 0L, 1L, 1L, "item", "other", 1L }),
        ("BIpd", new object[] { "YDWE", 0L, 1L, 1L, "item", "other", 1L }), ("Btlf", new object[] { "YDWE", 0L, 1L, 1L, "unit", "other", 1L }),
    };

    readonly Dictionary<string, Dictionary<string, WObj>> deflt = new();
    Dictionary<string, Dictionary<string, List<string>>> txtLeft = new(StringComparer.Ordinal);
    readonly Dictionary<string, Dictionary<string, WObj>> slk = new();
    readonly Dictionary<string, Dictionary<string, WObj>> residual = new();
    readonly Dictionary<string, string> skins = new(StringComparer.OrdinalIgnoreCase);
    public readonly List<string> Report = new();
    public int DroppedFields, UnknownParent, KeptAsObj, OverLevel, TxtEscape, Removed, Total;
    public bool RemoveUnused = true, ReforgedSkins = true;
    Func<string, string>? saveWts;

    public static SlkConv FromGame(GameFiles g)
    {
        var c = new SlkConv();
        c.BuildDefault(g.Read);
        return c;
    }

    void BuildDefault(Func<string, byte[]?> load)
    {
        var meta = MetaDb.Metadata;
        var keydata = MetaDb.KeyData;
        var txt = new Dictionary<string, Dictionary<string, List<string>>>(StringComparer.Ordinal);
        foreach (var f in TxtFiles)
        {
            var d = load(f);
            if (d != null) W3Txt.Parse(d, txt);
        }
        var used = new Dictionary<string, Dictionary<string, List<string>>?>(StringComparer.Ordinal);
        foreach (var type in Types)
        {
            var datas = new Dictionary<string, WObj>(StringComparer.Ordinal);
            LevelKey.TryGetValue(type, out var levelKey);
            foreach (var file in SlkFiles[type])
            {
                var keys = keydata.TryGetValue(file, out var kk) ? kk : new List<string>();
                var bytes = load(file) ?? throw new FileNotFoundException("нет в игре: " + file);
                var rows = W3Slk.Parse(bytes);
                if (file == @"units\abilitybuffdata.slk")
                    foreach (var (id, v) in YdweBuffs)
                    {
                        var r = new Dictionary<string, object?>(StringComparer.Ordinal);
                        for (int i = 0; i < YdweBuffCols.Length - 1; i++) r[YdweBuffCols[i]] = v[i];
                        rows[id] = r;
                    }
                foreach (var (name, data) in rows)
                {
                    if (!datas.TryGetValue(name, out var obj)) { obj = new WObj { Id = name, Type = type }; datas[name] = obj; }
                    if (data.TryGetValue("code", out var code) && code != null) obj.Code = LV.ToStr(code);
                    if (type == "unit" && obj.Name == null && data.TryGetValue("name", out var nm) && nm != null) obj.Name = LV.ToStr(nm);
                    foreach (var key in keys)
                        if (meta[type].TryGetValue(key, out var m)) SlkReadData(type, obj, key, m, data);
                }
                foreach (var (name, data) in rows)
                {
                    var obj = datas[name];
                    var priv = MetaDb.Of(obj.Code);
                    if (priv != null) foreach (var (key, m) in priv) SlkReadData(type, obj, key, m, data);
                }
                if (levelKey != null && keys.Contains(levelKey))
                    foreach (var name in rows.Keys)
                    {
                        var obj = datas[name];
                        obj.MaxLevel = obj.F.GetValueOrDefault(levelKey);
                        if (obj.MaxLevel == null || LV.Eq(obj.MaxLevel, 0L)) obj.MaxLevel = 1L;
                    }
            }
            if (keydata.TryGetValue(type, out var tkeys))
                foreach (var obj in datas.Values)
                {
                    var lname = obj.Id.ToLowerInvariant();
                    Dictionary<string, List<string>>? td;
                    if (txt.TryGetValue(lname, out var t1)) { td = t1; txt.Remove(lname); }
                    else td = used.GetValueOrDefault(lname);
                    used[lname] = td;
                    foreach (var key in tkeys)
                        if (meta[type].TryGetValue(key, out var m)) TxtReadData(obj, key, m, td);
                }
            if (type == "unit") datas.Remove("nrmf");
            deflt[type] = datas;
        }
        txtLeft = txt;
    }

    static void SlkReadData(string type, WObj obj, string key, Meta m, Dictionary<string, object?> data)
    {
        if (m.Repeat != null)
        {
            var t = new LTable();
            if (type == "doodad") for (int i = 1; i <= 10; i++) t[i] = SlkToType(m.Type, data.GetValueOrDefault(m.Field + i.ToString("00")));
            else for (int i = 1; i <= 4; i++) t[i] = SlkToType(m.Type, data.GetValueOrDefault(m.Field + i));
            obj.F[key] = t;
        }
        else obj.F[key] = SlkToType(m.Type, data.GetValueOrDefault(m.Field));
    }

    static object SlkToType(int tp, object? v)
    {
        switch (tp)
        {
            case 0: return v == null ? 0L : LV.Floor(LV.WToNumber(v));
            case 1:
            case 2:
                if (v == null) return 0.0;
                if (v is long l) return (double)l;
                if (v is double d) return d;
                var n = LV.WToNumber(v); return n is long nl ? (double)nl : (double)n;
            default:
                if (v == null) return "";
                var s = LV.ToStr(v);
                if (s.Length == 0) return s;
                foreach (char ch in s) if (ch != ' ' && ch != '-' && ch != '_') return s;
                return "";
        }
    }

    static object? TxtToType(int tp, string? v)
    {
        switch (tp)
        {
            case 0: return v == null ? 0L : LV.Floor(LV.WToNumber(v));
            case 1:
            case 2:
                if (v == null) return 0.0;
                var n = LV.WToNumber(v); return n is long nl ? (double)nl : (double)n;
            default: return v;
        }
    }

    static void TxtReadData(WObj obj, string key, Meta m, Dictionary<string, List<string>>? txt)
    {
        if (m.Index != null)
        {
            List<string>? value = null;
            if (txt != null) value = txt.GetValueOrDefault(m.MKey.ToLowerInvariant());
            value ??= m.Default;
            string? s = value != null && m.Index.Value - 1 < value.Count ? value[m.Index.Value - 1] : null;
            obj.F[key] = TxtToType(m.Type, s);
            return;
        }
        if (m.AppendIndex)
        {
            var t = new LTable();
            if (txt != null)
            {
                string? nul = null;
                int max = txt.TryGetValue(key + "count", out var cnt) && cnt.Count > 0 ? (int)LV.Floor(LV.WToNumber(cnt[0])) : 1;
                for (int i = max; i >= 1; i--)
                {
                    var nk = i == 1 ? key : key + (i - 1);
                    if (txt.TryGetValue(nk, out var val) && val.Count > 0) { t[i] = string.Join(",", val); nul = ""; }
                    else t[i] = nul;
                }
            }
            obj.F[key] = t;
            return;
        }
        List<string>? vals = txt?.GetValueOrDefault(key) ?? m.Default;
        if (vals == null || vals.Count == 0)
        {
            var v = TxtToType(m.Type, null);
            if (v == null)
            {
                if (m.Repeat != null) obj.F[key] = new LTable();
                return;
            }
            obj.F[key] = m.Repeat != null ? new LTable { [1] = v } : v;
            return;
        }
        if (m.Concat)
        {
            obj.F[key] = vals.Count > 1 ? string.Join(",", vals) : TxtToType(m.Type, vals[0]);
            return;
        }
        if (m.Repeat != null)
        {
            var t = new LTable();
            for (int i = 0; i < vals.Count; i++) t[i + 1] = TxtToType(m.Type, vals[i]);
            obj.F[key] = t;
        }
        else obj.F[key] = TxtToType(m.Type, vals[0]);
    }

    static LTable FillAndCopy(LTable a, long lv)
    {
        var c = new LTable();
        int n = a.Len();
        if (n < lv)
        {
            for (int i = 1; i <= n; i++) c[i] = a.At(i);
            for (long i = n + 1; i <= lv; i++) c[(int)i] = a.At(n);
        }
        else for (int i = 1; i <= lv; i++) c[i] = a.At(i);
        return c;
    }

    static LTable FillAndMerge(LTable a, LTable b, long lv, Meta? meta)
    {
        var c = new LTable();
        int n = a.Len();
        if (n < lv)
        {
            for (int i = 1; i <= n; i++) c[i] = b.At(i) ?? a.At(i);
            if (meta != null && meta.AppendIndex)
                for (int i = n + 1; i <= lv; i++) c[i] = b.At(i) ?? "";
            else if (meta == null || meta.Profile)
            {
                int maxlv = b.MaxIndex();
                for (int i = n + 1; i <= lv; i++)
                    c[i] = i > maxlv ? b.At(i) ?? c.At(i - 1) : b.At(i) ?? "";
            }
            else
                for (int i = n + 1; i <= lv; i++) c[i] = b.At(i) ?? a.At(n);
        }
        else for (int i = 1; i <= lv; i++) c[i] = b.At(i) ?? a.At(i);
        return c;
    }

    static void FillObj(WObj o)
    {
        if (o.MaxLevel == null) return;
        long lv = LV.Floor(o.MaxLevel);
        foreach (var k in o.F.Keys.ToList())
            if (o.F[k] is LTable t) o.F[k] = FillAndCopy(t, lv);
    }

    static WObj CopyObj(WObj a, WObj b)
    {
        var c = a.CloneShallowAttrs();
        c.Id = b.Id; c.Type = b.Type; c.Parent = b.Parent ?? a.Parent; c.IsObj = b.IsObj || a.IsObj;
        c.Code = b.Code ?? a.Code; c.Name = b.Name ?? a.Name; c.MaxLevel = b.MaxLevel ?? a.MaxLevel;
        c.KeepObj = b.KeepObj || a.KeepObj;
        long lv = LV.Floor(c.MaxLevel);
        var mCode = MetaDb.Of(a.Code); var mType = MetaDb.Of(a.Type);
        foreach (var (k, v) in a.F)
        {
            if (b.F.TryGetValue(k, out var bv) && bv != null)
            {
                if (v is LTable at)
                {
                    var meta = mCode?.GetValueOrDefault(k) ?? mType?.GetValueOrDefault(k);
                    c.F[k] = bv is LTable bt ? FillAndMerge(at, bt, lv, meta) : bv;
                }
                else c.F[k] = bv;
            }
            else c.F[k] = v is LTable at && c.MaxLevel != null ? FillAndCopy(at, lv) : v is LTable at2 ? new LTable(at2) : v;
        }
        foreach (var (k, v) in b.F)
            if (!a.F.ContainsKey(k)) c.F[k] = v;
        return c;
    }

    sealed class RawObj { public string Id = "", Parent = ""; public Dictionary<string, LTable> Mods = new(StringComparer.Ordinal); public byte[] Raw = Array.Empty<byte>(); public bool Original; }

    static readonly Regex Trig = new(@"TRIGSTR_(\d+)", RegexOptions.Compiled);

    static Dictionary<string, RawObj>? ReadObjFile(byte[] buf, bool hasLevel, Dictionary<int, string> wts)
    {
        var res = new Dictionary<string, RawObj>(StringComparer.Ordinal);
        try
        {
            var r = new BinaryReader(new MemoryStream(buf));
            int version = r.ReadInt32();
            for (int chunk = 0; chunk < 2; chunk++)
            {
                int count = r.ReadInt32();
                for (int o = 0; o < count; o++)
                {
                    long start = r.BaseStream.Position;
                    string parent = Encoding.Latin1.GetString(r.ReadBytes(4));
                    string name = Encoding.Latin1.GetString(r.ReadBytes(4));
                    if (name == "\0\0\0\0") name = parent;
                    var obj = new RawObj { Id = name, Parent = parent, Original = chunk == 0 };
                    int mods = r.ReadInt32();
                    for (int m = 0; m < mods; m++)
                    {
                        var idb = r.ReadBytes(4);
                        int z = Array.IndexOf(idb, (byte)0);
                        string id = Encoding.Latin1.GetString(idb, 0, z < 0 ? 4 : z);
                        int vt = r.ReadInt32();
                        int level = 0;
                        if (hasLevel) { level = r.ReadInt32(); r.ReadInt32(); }
                        object value;
                        if (vt == 0) value = (long)r.ReadInt32();
                        else if (vt == 1 || vt == 2) value = LV.Bin2Float(r.ReadSingle());
                        else if (vt == 3)
                        {
                            var sb = new List<byte>();
                            byte b;
                            while ((b = r.ReadByte()) != 0) sb.Add(b);
                            var s = Encoding.UTF8.GetString(sb.ToArray());
                            value = Trig.Replace(s, mt => wts.TryGetValue(int.Parse(mt.Groups[1].Value), out var t) ? t : mt.Value);
                        }
                        else return null;
                        r.ReadInt32();
                        if (level == 0) level = 1;
                        if (!obj.Mods.TryGetValue(id, out var lt)) { lt = new LTable(); obj.Mods[id] = lt; }
                        lt[level] = value;
                    }
                    long end = r.BaseStream.Position;
                    r.BaseStream.Position = start;
                    obj.Raw = r.ReadBytes((int)(end - start));
                    res[name] = obj;
                }
            }
            return r.BaseStream.Position == buf.Length ? res : null;
        }
        catch (EndOfStreamException) { return null; }
    }

    public sealed class Result
    {
        public Dictionary<string, byte[]> Write = new(StringComparer.OrdinalIgnoreCase);
        public HashSet<string> Remove = new(StringComparer.OrdinalIgnoreCase);
        public Dictionary<string, (int Before, int After)> ObjBytes = new();
    }

    public Result Convert(Func<string, byte[]?> get, Dictionary<int, string> wts, Func<string, string> saveWts)
    {
        this.saveWts = saveWts;
        var res = new Result();
        var passthrough = new Dictionary<string, List<RawObj>>();
        var objs = new Dictionary<string, Dictionary<string, WObj>>();
        foreach (var type in Types)
        {
            var buf = get(ObjFile[type]);
            var o = new Dictionary<string, WObj>(StringComparer.Ordinal);
            passthrough[type] = new List<RawObj>();
            if (buf != null)
            {
                var raw = ReadObjFile(buf, LevelKey.ContainsKey(type), wts) ?? throw new InvalidDataException("не разобран " + ObjFile[type]);
                foreach (var ro in raw.Values)
                {
                    var w = UpdateObj(type, ro);
                    if (w == null) { passthrough[type].Add(ro); UnknownParent++; continue; }
                    o[ro.Id] = w;
                }
            }
            if (type == "ability")
                foreach (var w in o.Values) if (w.F.Count == 0) w.KeepObj = true;
            slk[type] = Merge(deflt[type], o);
            var usedL = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var (id, w) in slk[type])
            {
                var lid = id.ToLowerInvariant();
                if (usedL.TryGetValue(lid, out var other)) { w.KeepObj = true; slk[type][other].KeepObj = true; }
                else if (!Regex.IsMatch(id, "^[A-Za-z0-9_]*$")) w.KeepObj = true;
                else usedL[lid] = id;
            }
        }
        foreach (var type in Types)
            foreach (var (name, w) in slk[type])
            {
                if (w.Parent != null && deflt[type].ContainsKey(w.Parent)) continue;
                if (deflt[type].TryGetValue(name, out var d) && d.Code == w.Code) { w.Parent = name; continue; }
                w.Parent = name;
            }
        if (RemoveUnused)
        {
            foreach (var type in Types) foreach (var w in slk[type].Values) w.Mark = false;
            MarkUsed(get);
            foreach (var type in Types)
            {
                foreach (var w in slk[type].Values)
                    if (w.KeepObj && w.Mark && w.Parent != null && slk[type].TryGetValue(w.Parent, out var p)) p.Mark = true;
                Removed += slk[type].Values.Count(w => !w.Mark);
                Total += slk[type].Count;
            }
        }
        foreach (var type in Types)
            foreach (var w in slk[type].Values) if (w.KeepObj) { CleanObj(type, w); KeptAsObj++; }

        foreach (var type in Types)
        {
            residual[type] = new Dictionary<string, WObj>(StringComparer.Ordinal);
            foreach (var file in SlkFiles[type])
                res.Write[file] = BackendSlk(type, file);
        }
        if (ReforgedSkins)
            foreach (var (path, text) in skins)
                if (get(path) == null) res.Write[path] = Encoding.UTF8.GetBytes(text);
                else Report.Add(path + ": уже есть в карте, не перезаписан");
        foreach (var f in TxtFiles) res.Write[f] = Array.Empty<byte>();
        foreach (var (type, text) in BackendTxt())
            res.Write[TxtOut[type]] = Encoding.UTF8.GetBytes(text);
        var extra = ExtraTxt();
        if (extra != null) res.Write[TxtOut["txt"]] = Encoding.UTF8.GetBytes(extra);
        foreach (var type in new[] { "ability", "buff", "unit", "item", "upgrade", "destructable", "doodad" })
        {
            var before = get(ObjFile[type]);
            var content = BackendObj(type, residual[type], passthrough[type]);
            if (content != null && content.Length > 0) res.Write[ObjFile[type]] = content;
            else if (before != null) res.Remove.Add(ObjFile[type]);
            res.ObjBytes[type] = (before?.Length ?? 0, content?.Length ?? 0);
        }
        return res;
    }

    WObj? UpdateObj(string type, RawObj ro)
    {
        if (!deflt[type].TryGetValue(ro.Parent, out var temp)) return null;
        var mods = new Dictionary<string, LTable>(ro.Mods, StringComparer.Ordinal);
        var w = new WObj { Id = ro.Id, Parent = ro.Parent, Type = type, IsObj = true, Code = temp.Code };
        void Take(Dictionary<string, Meta>? metas)
        {
            if (metas == null) return;
            foreach (var (key, m) in metas)
            {
                if (!mods.TryGetValue(m.Id, out var value)) continue;
                mods.Remove(m.Id);
                if (m.Splite)
                    foreach (var i in value.Keys.ToList())
                        if (value[i] is string s && s.IndexOf(',') is int p && p >= 0) value[i] = s[..p];
                w.F[key] = m.Repeat != null ? value : value.At(1);
            }
        }
        Take(MetaDb.Of(type));
        Take(MetaDb.Of(temp.Code));
        DroppedFields += mods.Count;
        if (LevelKey.TryGetValue(type, out var lk))
        {
            w.MaxLevel = w.F.GetValueOrDefault(lk);
            if (w.MaxLevel != null && LV.Eq(w.MaxLevel, 0L)) w.MaxLevel = 1L;
        }
        return w;
    }

    static Dictionary<string, WObj> Merge(Dictionary<string, WObj> data0, Dictionary<string, WObj> objs)
    {
        var data = new Dictionary<string, WObj>(data0, StringComparer.Ordinal);
        var template = new Dictionary<string, WObj>(StringComparer.Ordinal);
        var result = new Dictionary<string, WObj>(StringComparer.Ordinal);
        foreach (var (name, obj) in objs)
        {
            WObj? source;
            if (data.TryGetValue(name, out source)) { template[name] = source; data.Remove(name); }
            else source = template.GetValueOrDefault(obj.Parent!) ?? data.GetValueOrDefault(obj.Parent!);
            result[name] = CopyObj(source!, obj);
        }
        foreach (var (name, obj) in data)
        {
            var c = obj.CloneShallowAttrs();
            foreach (var (k, v) in obj.F) c.F[k] = v is LTable t ? new LTable(t) : v;
            FillObj(c);
            result[name] = c;
        }
        return result;
    }

    void CleanObj(string type, WObj obj)
    {
        if (obj.Parent == null || !deflt[type].TryGetValue(obj.Parent, out var d)) return;
        void Run(Dictionary<string, Meta>? metas)
        {
            if (metas == null) return;
            foreach (var (key, m) in metas)
            {
                var data = obj.F.GetValueOrDefault(key);
                if (m.Profile) SameAsTxt(m, key, data, d, obj); else SameAsSlk(m, key, data, d, obj);
            }
        }
        Run(MetaDb.Of(type));
        Run(MetaDb.Of(obj.Code));
    }

    static bool IsSame(object? a, object? b, Meta? m)
    {
        if (m != null && m.Type != 3 && a != null && b != null) { a = LV.WToNumber(a); b = LV.WToNumber(b); }
        return LV.Eq(a, b);
    }

    static object DefaultValue(int tp) => tp == 3 ? "" : 0L;

    static void SameAsSlk(Meta m, string key, object? data, WObj d, WObj obj)
    {
        var dest = d.F.GetValueOrDefault(key);
        if (dest is LTable dt)
        {
            if (data is not LTable t) return;
            var nd = new LTable();
            int dl = dt.Len();
            for (int i = 1; i <= t.Len(); i++)
            {
                var def = i > dl ? dt.At(dl) : dt.At(i);
                if (!IsSame(t.At(i), def, m)) nd[i] = t.At(i);
            }
            if (nd.Count == 0) obj.F[key] = nd;
        }
        else
        {
            if (LV.Eq(data, dest)) obj.F.Remove(key);
            else if (data == null) obj.F[key] = DefaultValue(m.Type);
        }
    }

    static void SameAsTxt(Meta m, string key, object? data, WObj d, WObj obj)
    {
        var dest = d.F.GetValueOrDefault(key);
        if (dest is LTable dt)
        {
            if (data is not LTable t) return;
            var nd = new LTable();
            if (m.AppendIndex)
            {
                for (int i = 1; i <= t.Len(); i++) if (!IsSame(t.At(i), dt.At(i) ?? "", m)) nd[i] = t.At(i);
            }
            else
            {
                bool valued = false;
                for (int i = t.Len(); i >= 1; i--)
                {
                    if (dt.At(i) == null)
                    {
                        if (valued || !IsSame(t.At(i), t.At(i - 1), m)) { nd[i] = t.At(i); valued = true; }
                    }
                    else if (!IsSame(t.At(i), dt.At(i), m)) { nd[i] = t.At(i); valued = true; }
                }
            }
            if (nd.Count == 0) obj.F[key] = nd;
        }
        else
        {
            if (IsSame(data, dest, m)) obj.F.Remove(key);
            else if (data == null) obj.F[key] = DefaultValue(m.Type);
        }
    }

    static string? SlkToTypeOut(int tp, object? value)
    {
        switch (tp)
        {
            case 0:
                {
                    if (value == null) return null;
                    var s = LV.Floor(value).ToString();
                    return s == "0" ? null : s;
                }
            case 1:
            case 2:
                if (value == null) return null;
                if (value is double d) return LV.ConvertReal(d);
                if (value is long l) return LV.ConvertReal(l);
                return LV.ToStr(value);
            default:
                {
                    if (value == null) return null;
                    var s = LV.ToStr(value);
                    if (s.Length == 0) return null;
                    bool any = false;
                    foreach (char ch in s) if (ch != ' ' && ch != '-' && ch != '_') { any = true; break; }
                    if (!any) return null;
                    if (Regex.IsMatch(s, @"^\.[mM][dD][lLxX]$")) return null;
                    return "\"" + s.Replace("\r\n", "|n").Replace("\r", "|n").Replace("\n", "|n") + "\"";
                }
        }
    }

    void LoadData(string type, Meta m, WObj obj, string key, Dictionary<string, string> slkData, WObj objData)
    {
        if (!obj.F.TryGetValue(key, out var v) || v == null) return;
        if (v is LTable t)
        {
            bool over = false;
            var od = new LTable();
            objData.F[key] = od;
            int top = type == "doodad" ? 10 : 4;
            int len = t.Len();
            for (int i = top + 1; i <= len; i++)
                if (!LV.Eq(t.At(i), t.At(top))) { od[i] = t.At(i); over = true; }
            for (int i = 1; i <= top; i++)
            {
                var s = SlkToTypeOut(m.Type, t.At(i));
                var col = type == "doodad" ? m.Field + i.ToString("00") : m.Field + i;
                if (s != null) slkData[col] = s; else slkData.Remove(col);
            }
            if (over) OverLevel++;
        }
        else
        {
            var s = SlkToTypeOut(m.Type, v);
            if (s != null) slkData[m.Field] = s; else slkData.Remove(m.Field);
        }
    }

    byte[] BackendSlk(string type, string file)
    {
        var keys = MetaDb.KeyData.TryGetValue(file, out var kk) ? kk : new List<string>();
        var skeys = SlkKeys[file];
        var tmeta = MetaDb.Metadata[type];
        var rows = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        foreach (var id in slk[type].Keys.OrderBy(k => k, StringComparer.Ordinal))
        {
            var obj = slk[type][id];
            if (!obj.Mark) continue;
            if (obj.KeepObj) { residual[type][id] = obj; continue; }
            if (!residual[type].TryGetValue(id, out var od))
            {
                od = new WObj { Id = obj.Id, Slk = true, Code = obj.Code, Mark = obj.Mark, Parent = obj.Parent, Type = type };
                residual[type][id] = od;
            }
            var sd = new Dictionary<string, string>(StringComparer.Ordinal);
            sd[skeys[0]] = "\"" + obj.Id + "\"";
            sd["code"] = "\"" + obj.Code + "\"";
            if (obj.Name != null)
                sd["name"] = "\"" + (obj.Id == obj.Parent ? obj.Name : "custom_" + obj.Id) + "\"";
            obj.Slk = true;
            foreach (var key in keys)
                if (tmeta.TryGetValue(key, out var m)) LoadData(type, m, obj, key, sd, od);
            var priv = MetaDb.Of(obj.Code);
            if (priv != null) foreach (var (key, m) in priv) LoadData(type, m, obj, key, sd, od);
            rows[id] = sd;
        }
        var d = deflt[type];
        var names = rows.Keys.ToList();
        names.Sort((a, b) =>
        {
            bool da = d.ContainsKey(a), db = d.ContainsKey(b);
            if (da != db) return da ? -1 : 1;
            return string.CompareOrdinal(a, b);
        });
        if (SkinOut.TryGetValue(file, out var so))
        {
            var st = new StringBuilder();
            foreach (var n in names)
            {
                st.Append('[').Append(n).Append("]\r\nskinType=").Append(so.Kind).Append("\r\n");
                foreach (var k in so.Keys)
                    if (rows[n].TryGetValue(k, out var v) && v.Trim('"') != "") st.Append(k).Append('=').Append(v.Trim('"')).Append("\r\n");
                st.Append("\r\n");
            }
            skins[so.Path] = st.ToString();
        }
        var sb = new StringBuilder();
        int? cx = null, cy = null;
        void Add(int x, int y, string k)
        {
            sb.Append('C');
            if (x != cx) { cx = x; sb.Append(";X").Append(x); }
            if (y != cy) { cy = y; sb.Append(";Y").Append(y); }
            sb.Append(";K").Append(k).Append("\r\n");
        }
        sb.Append("ID;PWXL;N;E\r\n");
        sb.Append("B;X").Append(skeys.Length).Append(";Y").Append(names.Count + 1).Append(";D0\r\n");
        for (int x = 0; x < skeys.Length; x++) Add(x + 1, 1, "\"" + skeys[x] + "\"");
        for (int y = 0; y < names.Count; y++)
        {
            var obj = rows[names[y]];
            for (int x = 0; x < skeys.Length; x++)
            {
                var key = skeys[x];
                if (obj.TryGetValue(key, out var value)) Add(x + 1, y + 2, value);
                else if (file == @"units\unitabilities.slk" && key == "auto"
                      || file == @"units\unitbalance.slk" && (key == "Primary" || key == "preventPlace" || key == "requirePlace")
                      || file == @"units\unitui.slk" && key == "file"
                      || file == @"units\destructabledata.slk" && key == "texFile"
                      || file == @"units\abilitydata.slk" && key.StartsWith("targs"))
                    Add(x + 1, y + 2, "\"_\"");
                else if (file == @"units\upgradedata.slk" && key == "used")
                    Add(x + 1, y + 2, "1");
            }
        }
        sb.Append('E');
        return Encoding.UTF8.GetBytes(sb.ToString());
    }

    static string? TxtTypeOut(int tp, object? value)
    {
        switch (tp)
        {
            case 0:
                {
                    if (value == null) return null;
                    var s = LV.Floor(value).ToString();
                    return s == "0" ? null : s;
                }
            case 1:
            case 2:
                {
                    if (value == null) return null;
                    string s = value is double d ? LV.ConvertReal(d) : value is long l ? LV.ConvertReal(l) : LV.ToStr(value);
                    if (s.Contains('.')) s = s.TrimEnd('0');
                    if (s.EndsWith('.')) s = s[..^1];
                    if (s == "" || s == "0") return null;
                    return s;
                }
            default:
                {
                    if (value == null) return null;
                    var s = LV.ToStr(value);
                    if (s.Contains(',')) s = "\"" + s + "\"";
                    return s;
                }
        }
    }

    static string? GetIndexData(int tp, object?[] l)
    {
        string? nul = null;
        var outv = new string?[l.Length];
        for (int i = l.Length - 1; i >= 0; i--)
        {
            var v = TxtTypeOut(tp, l[i]);
            if (v != null && v != "") { outv[i] = v; nul = ""; }
            else outv[i] = nul;
        }
        int len = 0;
        while (len < outv.Length && outv[len] != null) len++;
        if (len == 0) return null;
        if (tp == 3)
            while (len > 1 && outv[len - 1] == outv[len - 2]) len--;
        return string.Join(",", outv.Take(len));
    }

    static void AddTxtData(Dictionary<string, object?> obj, Meta m, object? value, List<(string K, object V)> kv)
    {
        var key = m.Field;
        if (m.Index != null)
        {
            if (m.Index == 1)
            {
                var mk = m.MKey;
                var v = GetIndexData(m.Type, new[] { obj.GetValueOrDefault(mk + "_1"), obj.GetValueOrDefault(mk + "_2") });
                if (v == null) { if (m.CantEmpty) v = ","; else return; }
                kv.Add((key[..^2], v));
            }
            return;
        }
        if (m.AppendIndex)
        {
            if (value is LTable t)
            {
                int len = t.MaxIndex();
                if (len == 0) return;
                if (len > 1) kv.Add((key + "count", (long)len));
                bool flag = false;
                for (int i = 1; i <= len; i++)
                {
                    var k = i > 1 ? key + (i - 1) : key;
                    var vi = t.At(i);
                    if (vi != null && !(vi is string s0 && s0 == ""))
                    {
                        flag = true;
                        var o = m.Concat ? vi : TxtTypeOut(m.Type, vi);
                        if (o != null) kv.Add((k, o)); else kv.Add((k, null!));
                    }
                }
                if (!flag && len > 1) kv.RemoveAt(kv.Count - 1);
            }
            else
            {
                if (value == null) return;
                var o = m.Concat ? value : TxtTypeOut(m.Type, value);
                if (o != null) kv.Add((key, o));
            }
            return;
        }
        if (m.Concat)
        {
            if (value != null && !LV.Eq(value, 0L)) kv.Add((key, value));
            return;
        }
        string? val;
        if (value is LTable lt)
        {
            int n = lt.Len();
            if (n == 0) return;
            var arr = new object?[n];
            for (int i = 0; i < n; i++) arr[i] = lt.At(i + 1);
            val = GetIndexData(m.Type, arr);
        }
        else val = TxtTypeOut(m.Type, value);
        if (string.IsNullOrEmpty(val))
        {
            if (m.CantEmpty) val = ","; else return;
        }
        kv.Add((key, val));
    }

    static bool CheckString(object? s) => s is string str && str.Contains(',') && str.Contains('"');

    List<(string Type, string Text)> BackendTxt()
    {
        var txt = new Dictionary<string, (string Type, Dictionary<string, object?> R)>(StringComparer.Ordinal);
        var list = new Dictionary<string, List<string>>();
        var txtTypes = new[] { "ability", "buff", "unit", "item", "upgrade" };
        foreach (var type in txtTypes)
        {
            list[type] = new List<string>();
            var keys = MetaDb.KeyData.GetValueOrDefault(type) ?? new List<string>();
            var md = MetaDb.Metadata[type];
            var objR = residual[type];
            foreach (var name in slk[type].Keys.OrderBy(k => k, StringComparer.Ordinal))
            {
                var obj = slk[type][name];
                if (!obj.Mark || obj.KeepObj) continue;
                var r = new Dictionary<string, object?>(StringComparer.Ordinal);
                foreach (var key in keys)
                {
                    if (!obj.F.TryGetValue(key, out var v) || v == null) continue;
                    if (v is LTable t)
                    {
                        var keep = new LTable(); var tt = new LTable();
                        foreach (var (k, iv) in t)
                            if (CheckString(iv)) { keep[k] = iv; TxtEscape++; } else tt[k] = iv;
                        if (keep.Count > 0) objR[name].F[key] = keep;
                        r[key] = tt;
                    }
                    else if (CheckString(v)) { objR[name].F[key] = v; TxtEscape++; }
                    else r[key] = v;
                }
                if (r.Count == 0) continue;
                r["_id"] = obj.Id;
                var lname = name.ToLowerInvariant();
                if (txt.TryGetValue(lname, out var prev))
                {
                    foreach (var (k, v) in r)
                    {
                        if (k == "_id") continue;
                        var a = prev.R.GetValueOrDefault(k);
                        if (v is LTable vt)
                        {
                            foreach (var (i, iv) in vt)
                                if (!(a is LTable at && LV.Eq(at.At(i), iv)))
                                {
                                    if (objR[name].F.GetValueOrDefault(k) is not LTable ot) { ot = new LTable(); objR[name].F[k] = ot; }
                                    ot[i] = iv;
                                }
                        }
                        else if (!LV.Eq(a, v)) objR[name].F[k] = v;
                    }
                }
                else { txt[lname] = (type, r); list[type].Add(obj.Id); }
            }
        }
        var res = new List<(string, string)>();
        foreach (var type in txtTypes)
        {
            var keys = MetaDb.KeyData.GetValueOrDefault(type) ?? new List<string>();
            var md = MetaDb.Metadata[type];
            var lines = new List<string>();
            var ids = list[type]; ids.Sort(string.CompareOrdinal);
            foreach (var id in ids)
            {
                var r = txt[id.ToLowerInvariant()].R;
                var kv = new List<(string K, object V)>();
                foreach (var key in keys)
                    if (key != "editorsuffix" && key != "editorname" && md.TryGetValue(key, out var m))
                        AddTxtData(r, m, r.GetValueOrDefault(key), kv);
                if (kv.Count == 0) continue;
                kv = kv.OrderBy(p => p.K.ToLowerInvariant(), StringComparer.Ordinal).ToList();
                lines.Add("[" + id + "]");
                bool empty = true;
                foreach (var (k, v) in kv)
                {
                    if (v == null) continue;
                    if (v is string s)
                    {
                        if (s == "") continue;
                        lines.Add(k + "=" + s.Replace("\r\n", "|n").Replace("\r", "|n").Replace("\n", "|n"));
                    }
                    else lines.Add(k + "=" + LV.ToStr(v));
                    empty = false;
                }
                if (empty) lines.RemoveAt(lines.Count - 1); else lines.Add("");
            }
            res.Add((type, string.Join("\r\n", lines)));
        }
        return res;
    }

    string? ExtraTxt()
    {
        if (txtLeft.Count == 0) return null;
        var lines = new List<string>();
        foreach (var name in txtLeft.Keys.OrderBy(k => k, StringComparer.Ordinal))
        {
            var vals = new List<string>();
            foreach (var key0 in txtLeft[name].Keys.OrderBy(k => k, StringComparer.Ordinal))
            {
                var data = txtLeft[name][key0];
                if (data.Count == 0) continue;
                var key = Regex.IsMatch(key0, @"[^A-Za-z0-9_]") ? "\"" + key0.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"" : key0;
                static string Fv(string v)
                {
                    v = v.Replace("\r\n", "|n").Replace("\r", "|n").Replace("\n", "|n");
                    return v.Contains(',') ? "\"" + v + "\"" : v;
                }
                var val = data.Count == 1 ? Fv(data[0]) : string.Join(",", data.Select(Fv));
                if (val == "" || key == "EditorSuffix" || key == "EditorName") continue;
                vals.Add(key + "=" + val);
            }
            if (vals.Count == 0) continue;
            lines.Add("[" + name + "]");
            lines.AddRange(vals);
            lines.Add("");
        }
        return string.Join("\r\n", lines);
    }

    byte[]? BackendObj(string type, Dictionary<string, WObj> data, List<RawObj> pass)
    {
        bool hasLevel = LevelKey.ContainsKey(type);
        var d = deflt[type];
        var origin = new List<string>(); var user = new List<string>();
        foreach (var (name, obj) in data)
        {
            if (!obj.Mark || name.Length != 4) continue;
            if (obj.Parent == null || !d.ContainsKey(obj.Parent)) continue;
            bool enable = (!obj.Slk && obj.Id != obj.Parent) || obj.KeepObj
                || obj.F.Any(p => p.Key[0] != '_' && (p.Value is LTable t ? t.Values.Any(x => x != null) : p.Value != null));
            if (!enable) continue;
            if (name == obj.Parent || obj.Slk) origin.Add(name); else user.Add(name);
        }
        var po = pass.Where(p => p.Original).ToList(); var pu = pass.Where(p => !p.Original).ToList();
        if (origin.Count + user.Count + pass.Count == 0) return null;
        origin.Sort((a, b) => string.CompareOrdinal(b, a));
        user.Sort((a, b) => string.CompareOrdinal(b, a));
        var ms = new MemoryStream();
        var w = new BinaryWriter(ms);
        w.Write(2);
        w.Write(origin.Count + po.Count);
        foreach (var n in origin) WriteObject(w, type, n, data[n], hasLevel);
        foreach (var p in po) w.Write(p.Raw);
        w.Write(user.Count + pu.Count);
        foreach (var n in user) WriteObject(w, type, n, data[n], hasLevel);
        foreach (var p in pu) w.Write(p.Raw);
        return ms.ToArray();
    }

    void WriteObject(BinaryWriter w, string type, string name, WObj obj, bool hasLevel)
    {
        var metas = new Dictionary<string, Meta>(StringComparer.Ordinal);
        if (MetaDb.Of(type) is { } mt) foreach (var (k, m) in mt) metas[k] = m;
        if (MetaDb.Of(obj.Code) is { } mc) foreach (var (k, m) in mc) metas[k] = m;
        var keys = obj.F.Keys.Where(k => k[0] != '_').ToList();
        keys.Sort(string.CompareOrdinal);
        var entries = new List<(Meta M, int Level, object? V)>();
        foreach (var key in keys)
        {
            var v = obj.F[key];
            if (v == null || !metas.TryGetValue(key, out var m)) continue;
            if (m.Repeat != null && v is not LTable) v = new LTable { [1] = v };
            if (v is LTable t)
            {
                int max = t.MaxIndex();
                for (int lvl = 1; lvl <= max; lvl++)
                    if (t.At(lvl) != null) entries.Add((m, lvl, t.At(lvl)));
            }
            else entries.Add((m, 0, v));
        }
        bool slkObj = name == obj.Parent || obj.Slk;
        w.Write(Encoding.Latin1.GetBytes(slkObj ? name : obj.Parent!.PadRight(4, '\0')[..4]));
        w.Write(slkObj ? new byte[4] : Encoding.Latin1.GetBytes(name));
        w.Write(entries.Count);
        foreach (var (m, lvl, v) in entries)
        {
            var idb = new byte[4];
            Encoding.Latin1.GetBytes(m.Id, 0, Math.Min(4, m.Id.Length), idb, 0);
            w.Write(idb);
            w.Write(m.Type);
            if (hasLevel) { w.Write(lvl); w.Write(m.Data); }
            if (m.Type == 0) w.Write((int)(v is long l ? l : LV.Floor(LV.WToNumber(v))));
            else if (m.Type == 1 || m.Type == 2) w.Write(LV.Float2Bin(v));
            else
            {
                string s = v as string ?? "";
                if (s.Contains('\0')) s = "";
                var bytes = Encoding.UTF8.GetBytes(s);
                if (bytes.Length > 1023) bytes = Encoding.UTF8.GetBytes(saveWts!(s));
                w.Write(bytes); w.Write((byte)0);
            }
            w.Write(0);
        }
    }
}
