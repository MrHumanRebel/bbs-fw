using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
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
        public void Save() { }
    }
}

class Program
{
    static void Check(bool condition, string description)
    {
        if (!condition) throw new Exception(description);
        Console.WriteLine("PASS: " + description);
    }

    static async Task Main()
    {
        await ControllerOperationTests.Run(Check);
        AdaptivePowerTests.Run(Check);
        byte[] telemetry = {1,4,1,1,224,1,4,3,232,1,24,1,1,0,0};
        telemetry[14] = (byte)telemetry.Take(14).Sum(x => x);
        var live = VirtualTelemetry.Decode(telemetry);
        Check(live != null && live.Watts == 1248 && live.Volts == 48 && live.Amps == 26,
              "telemetry reports uncapped measured power above 999 W");
        Check(Math.Abs(live.SpeedKph - 13.406) < 0.01, "RPM and configured wheel size convert to km/h");
        Check(VirtualTelemetry.Decode(telemetry.Take(14).ToArray()) == null, "partial telemetry rejected");
        telemetry[14] ^= 1;
        Check(VirtualTelemetry.Decode(telemetry) == null, "bad telemetry checksum rejected");
        var system = new ConfigurationViewModel();
        system.MaxCurrentAmps = 25;
        system.CurrentRampAmpsSecond = 10;
        Check(system.CurrentRiseSeconds == 2.5, "full rise time follows current limit and average rate");
        system.CurrentRiseSeconds = 1.25;
        Check(system.CurrentRampAmpsSecond == 20, "duration editing updates the encoded rate");
        system.CurrentRampSmoothnessPercent = 255;
        Check(system.CurrentRampSmoothnessPercent == 100, "view model bounds curve softness");
        system.UseVoltageCompensation = true;
        Check(system.GetConfig().UseVoltageCompensation && system.GetConfig().StandardAssistLevels.Concat(system.GetConfig().SportAssistLevels).All(l =>
            !l.Type.HasFlag((Configuration.AssistFlagsType)Configuration.LegacyVoltageCompensationFlag)),
            "one system toggle persists without per-level flags");
        int durationChanges = 0;
        system.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(system.CurrentRiseSeconds)) ++durationChanges; };
        system.MaxCurrentAmps = 30; system.CurrentRampAmpsSecond = 15;
        Check(durationChanges == 2 && system.CurrentRiseSeconds == 2, "curve duration notifies after either input changes");
        var cfg = new Configuration();
        cfg.StandardAssistLevels[1].Type = Configuration.AssistFlagsType.Pas | Configuration.AssistFlagsType.Throttle;
        cfg.MaxBatteryVolts = 58.8f;
        var original = cfg.WriteToBuffer();
        Check(original.Length == 156 && Configuration.CurrentVersion == 7, "v7 retains the 156-byte payload and existing global controls");
        Check(!cfg.UseVoltageCompensation && cfg.CurrentRampSmoothnessPercent == 0, "new controls default to compensation off and linear ramp");
        cfg.UseVoltageCompensation = true;
        cfg.CurrentRampSmoothnessPercent = 75;
        var encoded = cfg.WriteToBuffer();
        Check(encoded.Take(154).SequenceEqual(original.Take(154)), "v5 prefix remains unchanged by global options");
        var restored = new Configuration();
        Check(restored.ParseFromBufferV6(encoded) && restored.WriteToBuffer().SequenceEqual(encoded), "v6 binary round trip");
        Check(restored.UseVoltageCompensation && restored.CurrentRampSmoothnessPercent == 75, "global options restored");
        var confirmedSnapshot = new Configuration();
        confirmedSnapshot.CopyFrom(cfg);
        cfg.MaxCurrentAmps = 25;
        cfg.StandardAssistLevels[1].MaxCurrentPercent = 75;
        cfg.SportAssistLevels[2].MaxCurrentPercent = 45;
        Check(confirmedSnapshot.WriteToBuffer().SequenceEqual(encoded), "confirmed configuration snapshot keeps independent system and profile values");
        cfg.CopyFrom(confirmedSnapshot);
        encoded[155] = 101;
        Check(!new Configuration().ParseFromBufferV6(encoded), "invalid curve shape rejected");
        var legacy = original.Take(154).ToArray();
        legacy[34 + 6] |= 0x80;
        var migrated = new Configuration();
        Check(migrated.ParseFromBufferV5(legacy) && migrated.UseVoltageCompensation && migrated.CurrentRampSmoothnessPercent == 0,
              "v5 per-level flag migrates to global control and linear ramp");
        Check(migrated.StandardAssistLevels.Concat(migrated.SportAssistLevels).All(x => !x.Type.HasFlag((Configuration.AssistFlagsType)Configuration.LegacyVoltageCompensationFlag)), "legacy per-level flags cleared");
        string path = Path.GetTempFileName();
        try
        {
            cfg.WriteToFile(path);
            var fromXml = new Configuration(); fromXml.ReadFromFile(path);
            Check(fromXml.WriteToBuffer().SequenceEqual(cfg.WriteToBuffer()), "XML preserves global controls and every existing field");
            cfg.UseVoltageCompensation = false;
            cfg.CurrentRampSmoothnessPercent = 0;
            cfg.StandardAssistLevels[1].Type |= (Configuration.AssistFlagsType)Configuration.LegacyVoltageCompensationFlag;
            cfg.WriteToFile(path);
            File.WriteAllText(path, File.ReadAllText(path).Replace("PasAdaptive", "VoltageCompensation"));
            fromXml.ReadFromFile(path);
            Check(fromXml.UseVoltageCompensation, "legacy XML flags migrate to global control");
        }
        finally { File.Delete(path); }
        for (uint shape = 0; shape <= 100; ++shape)
        {
            int last = 0;
            for (int t = 0; t <= 1000; ++t)
            {
                int value = ResponseMath.Ramp(t, shape);
                if (value < last || value > 1000 || (shape == 0 && value != t)) throw new Exception("Invalid curve");
                last = value;
            }
            if (last != 1000) throw new Exception("Invalid curve endpoint");
        }
        Check(ResponseMath.Ramp(250, 100) == 156 && ResponseMath.Ramp(500, 100) == 500 && ResponseMath.Ramp(750, 100) == 843,
              "UI curve matches firmware integer reference values; all shapes monotonic");
    }
}
