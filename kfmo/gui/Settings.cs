using System.Text.Json;

namespace KFMO.Gui;

sealed class Settings
{
    public bool Slk { get; set; } = true;
    public bool RemoveUnused { get; set; } = true;
    public bool StripEditor { get; set; } = true;
    public int Models { get; set; } = 0;
    public bool KeyReduce { get; set; } = true;
    public bool IconOneMip { get; set; } = true;
    public bool JassMin { get; set; } = true;
    public int Textures { get; set; } = 0;
    public bool Listfile { get; set; } = true;
    public bool NoPreview { get; set; } = false;
    public int TexMax { get; set; } = 0;
    public string Suffix { get; set; } = "OPT";
    public string LastMap { get; set; } = "";
    public string GameDir { get; set; } = "";

    static string FilePath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "KFMO", "gui.json");

    public static Settings Load()
    {
        try
        {
            if (File.Exists(FilePath)) return JsonSerializer.Deserialize<Settings>(File.ReadAllText(FilePath)) ?? new Settings();
        }
        catch { }
        return new Settings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }
}
