using System;
using System.IO;
using System.Linq;
using BBSFW.Model;
using BBSFW.ViewModel;

internal static class ThermalConfigurationTests
{
    public static void Run(Action<bool, string> check)
    {
        var stock = new Configuration(BbsfwConnection.Controller.BBSHD);
        check(stock.UseTemperatureSensor == Configuration.TemperatureSensor.Controller &&
            stock.GetMotorTemperatureWarning(BbsfwConnection.Controller.BBSHD) == string.Empty,
            "stock BBSHD defaults to controller-only protection without requiring a motor thermistor");
        foreach (var selection in new[] { Configuration.TemperatureSensor.All, Configuration.TemperatureSensor.Motor })
        {
            stock.UseTemperatureSensor = selection;
            byte[] before = stock.WriteToBuffer();
            string warning = stock.GetMotorTemperatureWarning(BbsfwConnection.Controller.BBSHD);
            check(warning.Contains("presence is unverified") && warning.Contains("no additional sensor is required") &&
                warning.Contains("NTC/PTC") && warning.Contains("0x14") && before.SequenceEqual(stock.WriteToBuffer()),
                selection + " warns about unverified factory hardware without changing protection or bytes");
            check(stock.GetMotorTemperatureWarning(BbsfwConnection.Controller.Unknown) == warning &&
                stock.GetMotorTemperatureWarning(BbsfwConnection.Controller.BBS02) == string.Empty &&
                stock.GetMotorTemperatureWarning(BbsfwConnection.Controller.TSDZ2) == string.Empty,
                selection + " warns for unbound files without inventing motor inputs on other targets");
        }
        stock.UseTemperatureSensor = Configuration.TemperatureSensor.All;
        // Historical v2-v4 headers share the temperature byte at offset 10;
        // unused assist records are disabled, not reinterpreted as new flags.
        foreach (int version in new[] { 2, 3, 4 })
        {
            byte[] payload = new byte[Configuration.GetByteSize(version)];
            payload[1] = 25; payload[2] = 10;
            payload[3] = 0x54; payload[4] = 0x15; // 54.6 V full-charge header.
            payload[5] = 42; payload[6] = 25;
            payload[10] = (byte)Configuration.TemperatureSensor.All;
            var restored = new Configuration(BbsfwConnection.Controller.BBSHD);
            bool parsed = version == 2 ? restored.ParseFromBufferV2(payload) :
                version == 3 ? restored.ParseFromBufferV3(payload) : restored.ParseFromBufferV4(payload);
            check(parsed && restored.UseTemperatureSensor == Configuration.TemperatureSensor.All &&
                restored.GetMotorTemperatureWarning(BbsfwConnection.Controller.BBSHD).Contains("0x14"),
                "v" + version + " original temperature selection survives migration with the warning");
        }
        var oldest = new Configuration(BbsfwConnection.Controller.BBSHD)
        {
            UseTemperatureSensor = Configuration.TemperatureSensor.All
        };
        check(oldest.ParseFromBufferV1(new byte[Configuration.ByteSizeV1]) &&
            oldest.UseTemperatureSensor == Configuration.TemperatureSensor.Controller,
            "v1 without a temperature field defaults to stock controller-only sensing");
        byte[] saved = stock.WriteToBuffer();
        foreach (int version in new[] { 5, 6, 7, 8 })
        {
            var restored = new Configuration(BbsfwConnection.Controller.BBSHD);
            byte[] payload = saved.Take(Configuration.GetByteSize(version)).ToArray();
            // Profile bits were introduced in v8; use genuine historical options.
            if (version == 6 || version == 7) payload[154] &= 1;
            bool parsed = version switch
            {
                5 => restored.ParseFromBufferV5(payload),
                6 => restored.ParseFromBufferV6(payload),
                7 => restored.ParseFromBufferV7(payload),
                _ => restored.ParseFromBufferV8(payload)
            };
            check(parsed && restored.UseTemperatureSensor == Configuration.TemperatureSensor.All &&
                restored.GetMotorTemperatureWarning(BbsfwConnection.Controller.BBSHD).Contains("0x14"),
                "v" + version + " saved All remains enabled with the factory-verification warning");
        }
        var vm = new ConfigurationViewModel();
        int warningChanges = 0;
        vm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(vm.MotorTemperatureWarning)) ++warningChanges; };
        vm.UpdateFrom(stock);
        check(vm.HasMotorTemperatureWarning && warningChanges == 1,
            "reading a controller configuration immediately publishes the thermal warning");
        vm.UseTemperatureSensor = Configuration.TemperatureSensor.Controller;
        check(!vm.HasMotorTemperatureWarning && warningChanges == 2,
            "explicit controller-only selection clears the warning and notifies the UI");
        string path = Path.GetTempFileName();
        try
        {
            stock.WriteToFile(path);
            vm.ReadConfiguration(path);
            check(vm.UseTemperatureSensor == Configuration.TemperatureSensor.All && vm.HasMotorTemperatureWarning &&
                warningChanges == 3 && vm.GetConfig().WriteToBuffer().Length == 156,
                "old XML All is preserved and visibly warned after import without growing the format");
        }
        finally { File.Delete(path); }
    }
}
