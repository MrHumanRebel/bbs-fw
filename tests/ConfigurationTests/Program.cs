using System;
using System.IO;
using System.Linq;
using BBSFW.Model;
using BBSFW.ViewModel;

// Replace Windows settings and connection dependencies, not serialization logic.
namespace BBSFW.Model
{
    public class BbsfwConnection
    {
        public enum Controller { Unknown, BBSHD, BBS02, TSDZ2 }
    }
}
namespace BBSFW.Properties
{
    public class Settings
    {
        public static Settings Default = new Settings();
        public bool UseFreedomUnits = false;
    }
}
namespace BBSFW.ViewModel
{
    public class ConfigurationViewModel { public bool IsTorqueSensorSupported => true; }
}

class Program
{
    static void Check(bool condition, string description)
    {
        if (!condition) throw new Exception(description);
        Console.WriteLine("PASS: " + description);
    }

    static void Main()
    {
        byte[] telemetry = {1,4,1,1,224,1,4,3,232,1,24,1,1,0,0};
        telemetry[14] = (byte)telemetry.Take(14).Sum(x => x);
        var live = VirtualTelemetry.Decode(telemetry);
        Check(live != null && live.Watts == 1248 && live.Volts == 48 && live.Amps == 26,
              "telemetry reports uncapped measured power above 999 W");
        Check(Math.Abs(live.SpeedKph - 13.406) < 0.01, "RPM and configured wheel size convert to km/h");
        Check(VirtualTelemetry.Decode(telemetry.Take(14).ToArray()) == null, "partial telemetry rejected");
        telemetry[14] ^= 1;
        Check(VirtualTelemetry.Decode(telemetry) == null, "bad telemetry checksum rejected");
        var cfg = new Configuration();
        var level = cfg.StandardAssistLevels[1];
        level.Type = Configuration.AssistFlagsType.Pas | Configuration.AssistFlagsType.Throttle;
        level.MaxCurrentPercent = 40;
        cfg.MaxBatteryVolts = 58.8f;
        var vm = new AssistLevelViewModel(new ConfigurationViewModel(), 1, level);
        var original = cfg.WriteToBuffer();
        Check(original.Length == 154 && Configuration.CurrentVersion == 5, "existing v5 payload size preserved");
        Check(!vm.IsVoltageCompensationEnabled, "legacy configuration defaults to disabled");

        int notifications = 0;
        vm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(vm.IsVoltageCompensationEnabled)) notifications++; };
        vm.IsVoltageCompensationEnabled = true;
        var encoded = cfg.WriteToBuffer();
        Check(encoded.Zip(original, (a, b) => a ^ b).Count(x => x != 0) == 1 &&
              encoded.Zip(original, (a, b) => a ^ b).Single(x => x != 0) == 0x80,
              "enabling changes only bit 0x80 in the selected level");
        var restored = new Configuration();
        Check(restored.ParseFromBufferV5(encoded) && restored.WriteToBuffer().SequenceEqual(encoded), "binary round trip");
        Check(restored.StandardAssistLevels[1].Type.HasFlag(Configuration.AssistFlagsType.VoltageCompensation), "new flag restored from binary");

        string path = Path.GetTempFileName();
        try
        {
            cfg.WriteToFile(path);
            var fromXml = new Configuration();
            fromXml.ReadFromFile(path);
            Check(fromXml.StandardAssistLevels[1].Type == level.Type, "XML flag round trip");
        }
        finally { File.Delete(path); }

        vm.IsVoltageCompensationEnabled = false;
        Check(cfg.WriteToBuffer().SequenceEqual(original), "disabling restores the original payload exactly");
        Check(notifications == 2, "view model notifies on both changes");
        Check(restored.SportAssistLevels.All(x => !x.Type.HasFlag(Configuration.AssistFlagsType.VoltageCompensation)), "unselected levels stay disabled");
    }
}
