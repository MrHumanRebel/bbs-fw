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
        vm.MaxCurrentAmps = 50;
        vm.MaxBatteryVolts = 100;
        var level = vm.StandardAssistLevels[3];
        level.TargetCurrentPercent = 100;
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
        vm.MaxCurrentAmps = 25; vm.MaxBatteryVolts = 48; level.TargetCurrentPercent = 100;
        level.AdaptivePowerWatts = uint.MaxValue;
        check(level.AdaptivePowerWatts == 1200 && level.AdaptivePowerLimitWatts == 1200, "25 A at 48 V caps the budget at 1200 W");
        vm.MaxCurrentAmps = 33; vm.MaxBatteryVolts = 52;
        level.AdaptivePowerWatts = uint.MaxValue;
        check(level.AdaptivePowerWatts == 1710, "33 A at 52 V rounds the 1716 W ceiling downward to 1710 W");
        level.TargetCurrentPercent = 50;
        check(level.AdaptivePowerWatts == 850, "reducing level current immediately reduces the budget");
        level.AdaptivePowerWatts = 19;
        check(level.AdaptivePowerWatts == 10, "budget rounding never increases the requested watt limit");
        vm.MaxBatteryVolts = 48; vm.MaxCurrentAmps = 5;
        level.AdaptivePowerWatts = uint.MaxValue;
        check(level.AdaptivePowerWatts == 120, "global current and voltage changes propagate into the level ceiling");
        level.TargetCurrentPercent = 0;
        check(level.AdaptivePowerWatts == 0 && level.AdaptivePowerLimitWatts == 0, "zero current never silently becomes a positive budget");
        var motor = new Configuration(BbsfwConnection.Controller.TSDZ2) { MaxCurrentAmps = 33, MaxBatteryVolts = 52 };
        motor.StandardAssistLevels[1].MaxCurrentPercent = 100;
        check(motor.GetAdaptivePowerLimitWatts(motor.StandardAssistLevels[1]) == 1040, "TSDZ2 preview respects its 20 A platform ceiling even before validation");
        bool wrongControllerRejected = false;
        try { vm.GetConfig().MaxCurrentAmps = 25; vm.GetConfig().ValidateForController(BbsfwConnection.Controller.TSDZ2); }
        catch (Exception e) { wrongControllerRejected = e.Message.Contains("Connected controller"); }
        check(wrongControllerRejected, "writes validate against the actual connected controller");
        var valid = new Configuration(BbsfwConnection.Controller.BBSHD)
        {
            MaxCurrentAmps = 25, MaxBatteryVolts = 48, LowCutoffVolts = 40, CurrentRampAmpsSecond = 10,
            WheelSizeInch = 28, NumWheelSensorSignals = 1, PasStopDelayMilliseconds = 200,
            PasKeepCurrentPercent = 60, ThrottleStartMillivolts = 1000, ThrottleEndMillivolts = 3600,
            ShiftInterruptDuration = 600
        };
        var safetyLevel = valid.StandardAssistLevels[1];
        safetyLevel.Type = Configuration.AssistFlagsType.Pas | Configuration.AssistFlagsType.PasAdaptive;
        safetyLevel.MaxCurrentPercent = 100; safetyLevel.TorqueAmplificationFactor = 12;
        valid.ValidateForController(BbsfwConnection.Controller.BBSHD);
        safetyLevel.TorqueAmplificationFactor = 12.1f;
        bool excessRejected = false;
        try { valid.Validate(); } catch (Exception e) { excessRejected = e.Message.Contains("ceiling"); }
        check(excessRejected, "configuration validation rejects manually injected budgets above current times voltage");
        vm.MaxCurrentAmps = 25; vm.MaxBatteryVolts = 48;
        foreach (var entry in vm.StandardAssistLevels.Concat(vm.SportAssistLevels))
        {
            entry.SelectedType = entry.AssistBaseTypeOptions.First(x => x.Value == AssistLevelViewModel.AssistBaseType.Pas);
            entry.TargetCurrentPercent = 100;
            entry.SelectedPasVariant = entry.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.AdaptivePower);
            entry.AdaptivePowerWatts = 1000;
        }
        vm.MaxCurrentAmps = 5;
        check(vm.StandardAssistLevels.Concat(vm.SportAssistLevels).All(x => x.AdaptivePowerWatts == 240),
            "global current reduction bounds every Standard and Sport level");
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
