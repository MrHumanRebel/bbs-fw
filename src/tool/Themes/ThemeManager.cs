using System;
using System.IO;
using System.Windows;

namespace BBSFW.Themes
{
    internal static class ThemeManager
    {
        private static readonly string PreferencePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "BBSFWTool", "theme.txt");
        private static ResourceDictionary palette;
        public static event Action Changed;
        public static bool IsDark { get; private set; } = true;

        public static void Initialize()
        {
            bool dark = true;
            try { dark = !File.Exists(PreferencePath) || File.ReadAllText(PreferencePath).Trim() != "light"; }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            Apply(dark, false);
        }

        public static void Apply(bool dark, bool save = true)
        {
            var next = new ResourceDictionary
            {
                Source = new Uri($"/BBSFWTool;component/Themes/{(dark ? "Dark" : "Light")}.xaml", UriKind.Relative)
            };
            var dictionaries = Application.Current.Resources.MergedDictionaries;
            if (palette != null) dictionaries.Remove(palette);
            dictionaries.Insert(0, next);
            palette = next;
            IsDark = dark;
            Changed?.Invoke();
            if (!save) return;
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(PreferencePath));
                File.WriteAllText(PreferencePath, dark ? "dark" : "light");
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }
}
