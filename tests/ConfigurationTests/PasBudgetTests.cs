using System;
using System.IO;
using System.Linq;
using BBSFW.Model;
using BBSFW.ViewModel;

internal static class PasBudgetTests
{
    public static void Run(Action<bool, string> check)
    {
        var vm = new ConfigurationViewModel();
        vm.MaxCurrentAmps = 50;
        vm.MaxBatteryVolts = 100;
        var level = vm.StandardAssistLevels[3];
        level.TargetCurrentPercent = 100;
        level.SelectedType = level.AssistBaseTypeOptions.First(x => x.Value == AssistLevelViewModel.AssistBaseType.Pas);
        level.SelectedPasVariant = level.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.AdaptiveEfficiency);
        check(level.IsPasAssistAdaptiveVariant && level.PasBudgetWatts == 250, "Adaptive Efficiency is a PAS variant with its own per-level budget");
        level.IsThrottleEnabled = true;
        level.MaxThrottlePercent = 40;
        level.PasBudgetWatts = 123;
        check(level.PasBudgetWatts == 120 && level.IsThrottleEnabled && level.MaxThrottlePercent == 40,
            "budget uses 10 W steps and preserves independent throttle settings");
        level.PasBudgetWatts = uint.MaxValue;
        check(level.PasBudgetWatts == 2550, "per-level power budget fits its existing byte");
        level.PasBudgetWatts = 10;
        var cfg = vm.GetConfig();
        byte[] payload = cfg.WriteToBuffer();
        var parsed = new Configuration();
        check(payload.Length == 156 && Configuration.GetByteSize(7) == 156 && parsed.ParseFromBufferV8(payload) && parsed.WriteToBuffer().SequenceEqual(payload),
            "current efficiency binary round trip retains the exact configuration size");
        check(parsed.StandardAssistLevels[3].Type.HasFlag(Configuration.AssistFlagsType.PasAdaptive) && !parsed.UseVoltageCompensation,
            "efficiency budget bit does not enable global voltage compensation");
        for (uint step = 1; step <= 255; ++step)
        {
            level.PasBudgetWatts = step * 10;
            byte[] bytes = cfg.WriteToBuffer();
            if (bytes[34 + 3 * 6 + 5] != step || !new Configuration().ParseFromBufferV8(bytes))
                throw new Exception("Adaptive budget encoding changed");
        }
        level.PasBudgetWatts = 10;
        check(true, "all 255 budgets encode exactly in the reused parameter byte");
        var old = new Configuration();
        check(old.ParseFromBufferV6(payload) && !old.StandardAssistLevels[3].Type.HasFlag(Configuration.AssistFlagsType.PasAdaptive),
            "old binary versions cannot silently activate the new variant");
        var invalid = payload.ToArray();
        invalid[34 + 3 * 6 + 5] = 0;
        check(!new Configuration().ParseFromBufferV8(invalid), "zero adaptive budget is rejected");
        invalid = payload.ToArray();
        invalid[34 + 3 * 6] |= (byte)Configuration.AssistFlagsType.PasTorque;
        check(!new Configuration().ParseFromBufferV8(invalid), "mixed adaptive and torque variants are rejected");
        string path = Path.GetTempFileName();
        try
        {
            cfg.WriteToFile(path);
            using (var exclusive = new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                check(exclusive.Length > 0, "XML save releases the file for immediate exclusive access");
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
                "old XML compensation name migrates without selecting Adaptive Efficiency");
        }
        finally { File.Delete(path); }
        vm.MaxCurrentAmps = 25; vm.MaxBatteryVolts = 48; level.TargetCurrentPercent = 100;
        level.PasBudgetWatts = uint.MaxValue;
        check(level.PasBudgetWatts == 1200 && level.PasBudgetLimitWatts == 1200, "25 A at 48 V caps the budget at 1200 W");
        vm.MaxCurrentAmps = 33; vm.MaxBatteryVolts = 52;
        level.PasBudgetWatts = uint.MaxValue;
        check(level.PasBudgetWatts == 1710, "33 A at 52 V rounds the 1716 W ceiling downward to 1710 W");
        level.TargetCurrentPercent = 50;
        check(level.PasBudgetWatts == 850, "reducing level current immediately reduces the budget");
        level.PasBudgetWatts = 19;
        check(level.PasBudgetWatts == 10, "budget rounding never increases the requested watt limit");
        vm.MaxBatteryVolts = 48; vm.MaxCurrentAmps = 5;
        level.PasBudgetWatts = uint.MaxValue;
        check(level.PasBudgetWatts == 120, "global current and voltage changes propagate into the level ceiling");
        level.TargetCurrentPercent = 0;
        check(level.PasBudgetWatts == 0 && level.PasBudgetLimitWatts == 0, "zero current never silently becomes a positive budget");
        var motor = new Configuration(BbsfwConnection.Controller.TSDZ2) { MaxCurrentAmps = 33, MaxBatteryVolts = 52 };
        motor.StandardAssistLevels[1].MaxCurrentPercent = 100;
        check(motor.GetPasBudgetLimitWatts(motor.StandardAssistLevels[1]) == 1040, "TSDZ2 preview respects its 20 A platform ceiling even before validation");
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
            entry.SelectedPasVariant = entry.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.AdaptiveEfficiency);
            entry.PasBudgetWatts = 1000;
        }
        vm.MaxCurrentAmps = 5;
        check(vm.StandardAssistLevels.Concat(vm.SportAssistLevels).All(x => x.PasBudgetWatts == 240),
            "global current reduction bounds every Standard and Sport level");
        level.SelectedPasVariant = level.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.Cadence);
        check(!level.IsPasAssistAdaptiveVariant && level.TorqueAmplificationFactor == 0, "leaving Adaptive Efficiency clears its variant and shared parameter");
        for (uint watts = 10; watts <= 2550; watts += 10)
            for (int cadence = 0; cadence <= 1500; ++cadence)
            {
                int value = ResponseMath.PasBudgetTenths(cadence, watts);
                if (value != (int)(watts / 10) * Math.Clamp(cadence, 150, 600) / 6 || value > watts * 10)
                    throw new Exception("Invalid adaptive preview");
            }
        check(ResponseMath.PasBudgetTenths(300, 250) == 1250 && ResponseMath.PasBudgetTenths(600, 250) == 2500,
            "preview power matches firmware integer envelope across budgets and cadences");
    }
}
