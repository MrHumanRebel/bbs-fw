using System;
using System.IO;
using System.Linq;
using BBSFW.Model;
using BBSFW.ViewModel;

internal static class AdaptivePowerTests
{
    public static void Run(Action<bool, string> check)
    {
        var vm = new ConfigurationViewModel();
        var level = vm.StandardAssistLevels[3];
        level.SelectedType = level.AssistBaseTypeOptions.First(x => x.Value == AssistLevelViewModel.AssistBaseType.Pas);
        level.SelectedPasVariant = level.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.AdaptivePower);
        check(level.IsPasAssistAdaptiveVariant && level.AdaptivePowerWatts == 250, "Adaptive Power is a PAS variant with its own per-level budget");
        level.IsThrottleEnabled = true;
        level.MaxThrottlePercent = 40;
        level.AdaptivePowerWatts = 123;
        check(level.AdaptivePowerWatts == 120 && level.IsThrottleEnabled && level.MaxThrottlePercent == 40,
            "budget uses 10 W steps and preserves independent throttle settings");
        level.AdaptivePowerWatts = uint.MaxValue;
        check(level.AdaptivePowerWatts == 2550, "per-level power budget fits its existing byte");
        level.AdaptivePowerWatts = 10;
        var cfg = vm.GetConfig();
        byte[] payload = cfg.WriteToBuffer();
        var parsed = new Configuration();
        check(payload.Length == 156 && Configuration.GetByteSize(7) == 156 && parsed.ParseFromBufferV7(payload) && parsed.WriteToBuffer().SequenceEqual(payload),
            "v7 adaptive binary round trip retains the exact configuration size");
        check(parsed.StandardAssistLevels[3].Type.HasFlag(Configuration.AssistFlagsType.PasAdaptive) && !parsed.UseVoltageCompensation,
            "v7 adaptive bit does not enable global voltage compensation");
        for (uint step = 1; step <= 255; ++step)
        {
            level.AdaptivePowerWatts = step * 10;
            byte[] bytes = cfg.WriteToBuffer();
            if (bytes[34 + 3 * 6 + 5] != step || !new Configuration().ParseFromBufferV7(bytes))
                throw new Exception("Adaptive budget encoding changed");
        }
        level.AdaptivePowerWatts = 10;
        check(true, "all 255 budgets encode exactly in the reused parameter byte");
        var old = new Configuration();
        check(old.ParseFromBufferV6(payload) && !old.StandardAssistLevels[3].Type.HasFlag(Configuration.AssistFlagsType.PasAdaptive),
            "old binary versions cannot silently activate the new variant");
        var invalid = payload.ToArray();
        invalid[34 + 3 * 6 + 5] = 0;
        check(!new Configuration().ParseFromBufferV7(invalid), "zero adaptive budget is rejected");
        invalid = payload.ToArray();
        invalid[34 + 3 * 6] |= (byte)Configuration.AssistFlagsType.PasTorque;
        check(!new Configuration().ParseFromBufferV7(invalid), "mixed adaptive and torque variants are rejected");
        string path = Path.GetTempFileName();
        try
        {
            cfg.WriteToFile(path);
            var restored = new Configuration(); restored.ReadFromFile(path);
            check(restored.WriteToBuffer().SequenceEqual(payload), "new XML retains adaptive settings without legacy compensation migration");
            string validXml = File.ReadAllText(path);
            cfg.StandardAssistLevels[3].TorqueAmplificationFactor = 0.15f;
            cfg.WriteToFile(path);
            bool rejected = false;
            try { new Configuration().ReadFromFile(path); }
            catch (InvalidDataException) { rejected = true; }
            check(rejected, "XML budgets between 10 W steps are rejected instead of silently rounded upward");
            cfg.StandardAssistLevels[3].TorqueAmplificationFactor = 0.1f;
            string legacy = validXml.Replace("PasAdaptive", "VoltageCompensation");
            File.WriteAllText(path, legacy);
            restored.ReadFromFile(path);
            check(restored.UseVoltageCompensation && !restored.StandardAssistLevels[3].Type.HasFlag(Configuration.AssistFlagsType.PasAdaptive),
                "old XML compensation name migrates without selecting Adaptive Power");
        }
        finally { File.Delete(path); }
        level.SelectedPasVariant = level.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.Cadence);
        check(!level.IsPasAssistAdaptiveVariant && level.TorqueAmplificationFactor == 0, "leaving Adaptive Power clears its variant and shared parameter");
        for (uint watts = 10; watts <= 2550; watts += 10)
            for (int cadence = 0; cadence <= 1500; ++cadence)
            {
                int value = ResponseMath.AdaptivePowerTenths(cadence, watts);
                if (value != (int)(watts / 10) * Math.Clamp(cadence, 150, 600) / 6 || value > watts * 10)
                    throw new Exception("Invalid adaptive preview");
            }
        check(ResponseMath.AdaptivePowerTenths(300, 250) == 1250 && ResponseMath.AdaptivePowerTenths(600, 250) == 2500,
            "preview power matches firmware integer envelope across budgets and cadences");
    }
}
