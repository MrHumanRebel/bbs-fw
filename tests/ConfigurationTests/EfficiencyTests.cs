using System;
using System.IO;
using System.Linq;
using BBSFW.Model;
using BBSFW.ViewModel;

internal static class EfficiencyTests
{
    public static void Run(Action<bool, string> check)
    {
        check(ConfigurationViewModel.EfficiencyProfiles.Select(x => x.ToString()).SequenceEqual(new[] {"BBS02B", "BBSHD", "TSDZ2"}),
            "exactly three motor efficiency profile choices");
        foreach (var profile in ConfigurationViewModel.EfficiencyProfiles)
        {
            var controller = profile == Configuration.MotorEfficiencyProfile.BBS02B ? BbsfwConnection.Controller.BBS02 :
                profile == Configuration.MotorEfficiencyProfile.BBSHD ? BbsfwConnection.Controller.BBSHD : BbsfwConnection.Controller.TSDZ2;
            var cfg = new Configuration(controller)
            {
                MaxCurrentAmps = 20, MaxBatteryVolts = 48, LowCutoffVolts = 40, CurrentRampAmpsSecond = 10,
                WheelSizeInch = 28, NumWheelSensorSignals = 1, PasStopDelayMilliseconds = 200,
                PasKeepCurrentPercent = 60, ThrottleStartMillivolts = 1000, ThrottleEndMillivolts = 3600,
                ShiftInterruptDuration = 600, UseVoltageCompensation = true, AdaptiveComfort = true
            };
            cfg.StandardAssistLevels[1].Type = Configuration.AssistFlagsType.Pas | Configuration.AssistFlagsType.PasAdaptive | Configuration.AssistFlagsType.PasVariable;
            cfg.StandardAssistLevels[1].MaxCurrentPercent = 100;
            cfg.StandardAssistLevels[1].TorqueAmplificationFactor = 2.5f;
            cfg.ValidateForController(controller);
            byte[] encoded = cfg.WriteToBuffer();
            var restored = new Configuration();
            check(encoded.Length == 156 && encoded[154] == (1 | ((int)profile << 1) | 8) && restored.ParseFromBufferV8(encoded) &&
                restored.WriteToBuffer().SequenceEqual(encoded), profile + " packs options into the existing byte and round trips");
            check(!new Configuration().ParseFromBufferV7(encoded), "v7 decoder cannot silently reinterpret v8 " + profile);
            bool refused = false;
            var wrongController = controller == BbsfwConnection.Controller.BBSHD ? BbsfwConnection.Controller.BBS02 : BbsfwConnection.Controller.BBSHD;
            try { cfg.ValidateForController(wrongController); } catch (Exception e) { refused = e.Message.Contains("profile"); }
            check(refused, profile + " efficiency settings reject incompatible connected hardware");
            string path = Path.GetTempFileName();
            try
            {
                cfg.WriteToFile(path); restored.ReadFromFile(path);
                check(restored.WriteToBuffer().SequenceEqual(encoded), profile + " XML preserves explicit variant, profile and comfort");
            }
            finally { File.Delete(path); }
            encoded[154] = 6;
            check(!new Configuration().ParseFromBufferV8(encoded), "reserved profile rejected for " + profile);
            encoded[154] = 16;
            check(!new Configuration().ParseFromBufferV8(encoded), "unknown option bits rejected for " + profile);
            // Construct genuine v7 power-budget encoding, not a new combined variant.
            encoded[154] = 1; encoded[34 + 6] &= unchecked((byte)~(byte)Configuration.AssistFlagsType.PasVariable);
            check(restored.ParseFromBufferV7(encoded) && !restored.AdaptiveComfort &&
                Configuration.IsEfficiencyLevel(restored.StandardAssistLevels[1]), "v7 power migration selects the sole Efficiency controller without comfort for " + profile);
            encoded[34 + 6] |= (byte)Configuration.AssistFlagsType.PasVariable;
            check(!new Configuration().ParseFromBufferV7(encoded), "corrupt v7 combined variant never becomes efficiency for " + profile);
        }
        var vm = new ConfigurationViewModel();
        vm.MaxCurrentAmps = 25; vm.MaxBatteryVolts = 48; vm.MaxSpeedKph = 100;
        var level = vm.StandardAssistLevels[1];
        level.SelectedType = level.AssistBaseTypeOptions.First(x => x.Value == AssistLevelViewModel.AssistBaseType.Pas);
        level.TargetCurrentPercent = 100;
        level.MaxSpeedPercent = 20;
        level.SelectedPasVariant = level.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.AdaptiveEfficiency);
        level.PasBudgetWatts = 10;
        check(level.IsPasAssistEfficiencyVariant && !level.IsPasAssistVariableVariant && level.IsPasAssistAdaptiveVariant,
            "Adaptive Efficiency is an explicit PAS variant, not a global ECO toggle");
        check(level.AdaptiveQuantizationWarning.Contains("zero current") && level.AdaptiveQuantizationWarning.Contains("throttle"),
            "10 W quantization warning and independent throttle disclosure are visible");
        check(level.AdaptiveSpeedText.Contains("20.0 km/h") || level.AdaptiveSpeedText.Contains("20,0 km/h"),
            "speed explanation uses global maximum times selected level percentage");
        level.PasBudgetWatts = 40;
        check(level.AdaptiveQuantizationWarning.Contains("Low-cadence"), "launch quantization warning accounts for 25% budget floor");
        vm.MaxCurrentAmps = 5;
        check(!level.AdaptiveQuantizationWarning.Contains("zero current"), "quantization warning updates after global current changes");
        level.SelectedPasVariant = level.AssistPasVariantOptions.First(x => x.Value == AssistLevelViewModel.AssistPasVariant.Cadence);
        check(!level.IsPasAssistEfficiencyVariant && !level.IsPasAssistVariableVariant, "leaving efficiency clears both reused variant bits");
        check(!new Configuration().AdaptiveComfort, "comfort defaults OFF");
        check(level.AssistPasVariantOptions.Count(x => x.Value == AssistLevelViewModel.AssistPasVariant.AdaptiveEfficiency) == 1 &&
            level.AssistPasVariantOptions.All(x => x.Name != "Adaptive Power" && !x.Name.Contains("experimental", StringComparison.OrdinalIgnoreCase)) &&
            ConfigurationViewModel.EfficiencyProfiles.All(profile =>
                !new ConfigurationViewModel { EfficiencyProfile = profile }.EfficiencyProfileDescription.Contains("experimental", StringComparison.OrdinalIgnoreCase)),
            "only Adaptive Efficiency is selectable, without an experimental label");
        foreach (var controller in new[] { BbsfwConnection.Controller.BBS02, BbsfwConnection.Controller.BBSHD, BbsfwConnection.Controller.TSDZ2 })
        {
            var fresh = new Configuration(controller);
            check(fresh.UseTemperatureSensor == (controller == BbsfwConnection.Controller.TSDZ2 ?
                Configuration.TemperatureSensor.Disabled : Configuration.TemperatureSensor.Controller),
                controller + " defaults only to physically supported controller temperature protection");
        }
        var historical = new Configuration(BbsfwConnection.Controller.BBSHD)
        {
            MaxCurrentAmps = 25, MaxBatteryVolts = 48,
            UseTemperatureSensor = Configuration.TemperatureSensor.All
        };
        historical.StandardAssistLevels[1].Type = Configuration.AssistFlagsType.Pas | Configuration.AssistFlagsType.PasAdaptive;
        historical.StandardAssistLevels[1].MaxCurrentPercent = 100;
        historical.StandardAssistLevels[1].MaxSpeedPercent = 80;
        historical.StandardAssistLevels[1].TorqueAmplificationFactor = 2.5f;
        byte[] previous = historical.WriteToBuffer();
        previous[34 + 6] &= unchecked((byte)~(byte)Configuration.AssistFlagsType.PasVariable);
        previous[154] = 0; // Old power-only settings did not require a matching selected profile.
        var migrated = new Configuration(BbsfwConnection.Controller.BBSHD);
        check(migrated.ParseFromBufferV8(previous) && Configuration.IsEfficiencyLevel(migrated.StandardAssistLevels[1]) &&
            migrated.EfficiencyProfile == Configuration.MotorEfficiencyProfile.BBSHD &&
            migrated.StandardAssistLevels[1].MaxSpeedPercent == 80 && migrated.StandardAssistLevels[1].TorqueAmplificationFactor == 2.5f &&
            migrated.UseTemperatureSensor == Configuration.TemperatureSensor.All,
            "retired power-only binary settings migrate to Efficiency with preserved limits and explicit temperature choices");
        check((migrated.WriteToBuffer()[34 + 6] & 0x88) == 0x88,
            "writes always encode the sole Efficiency variant, never the retired power-only mode");
    }
}
