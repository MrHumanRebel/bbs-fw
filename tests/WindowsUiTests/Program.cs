using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using BBSFW;
using BBSFW.View.Controls;
using BBSFW.ViewModel;

internal static class Program
{
    private static void Check(bool value, string description)
    {
        if (!value) throw new InvalidOperationException(description);
        Console.WriteLine("PASS: " + description);
    }
    private static IEnumerable<DependencyObject> Descendants(DependencyObject parent)
    {
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(parent); ++i)
        {
            var child = VisualTreeHelper.GetChild(parent, i);
            yield return child;
            foreach (var nested in Descendants(child)) yield return nested;
        }
    }
    private static void Pump()
    {
        var frame = new DispatcherFrame();
        var timer = new DispatcherTimer(DispatcherPriority.Background) { Interval = TimeSpan.FromMilliseconds(200) };
        timer.Tick += (_, _) => { timer.Stop(); frame.Continue = false; };
        timer.Start(); Dispatcher.PushFrame(frame);
    }
    private static Rect Bounds(FrameworkElement element, Visual root) =>
        element.TransformToAncestor(root).TransformBounds(new Rect(element.RenderSize));
    private static void Screenshot(FrameworkElement content, string path)
    {
        var bitmap = new RenderTargetBitmap(Math.Max(1, (int)content.ActualWidth), Math.Max(1, (int)content.ActualHeight), 96, 96, PixelFormats.Pbgra32);
        bitmap.Render(content);
        var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using var file = File.Create(path); encoder.Save(file);
    }
    [STAThread]
    public static int Main(string[] args)
    {
        string output = args.Length > 0 ? args[0] : "ui-preview";
        Directory.CreateDirectory(output);
        var app = new BBSFW.App { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        app.InitializeComponent();
        var theme = typeof(MainWindow).Assembly.GetType("BBSFW.Themes.ThemeManager");
        theme.GetMethod("Initialize", BindingFlags.Static | BindingFlags.Public).Invoke(null, null);
        var window = new MainWindow();
        int confirmations = 0;
        bool approveWrites = false;
        Action onConfirmation = null;
        var vm = new MainViewModel((_, _) => { ++confirmations; onConfirmation?.Invoke(); return approveWrites; });
        window.DataContext = vm;
        vm.ConfigVm.MaxCurrentAmps = 25;
        vm.ConfigVm.MaxBatteryVolts = 58.8f;
        vm.ConfigVm.LowCutoffVolts = 42;
        vm.ConfigVm.MaxSpeedKph = 25;
        vm.ConfigVm.UseSpeedSensor = true;
        vm.ConfigVm.WheelSizeInch = 28;
        vm.ConfigVm.SpeedSensorSignals = 1;
        vm.ConfigVm.GetConfig().StandardAssistLevels[1].Type = BBSFW.Model.Configuration.AssistFlagsType.Pas;
        vm.ConfigVm.StandardAssistLevels[1].TargetCurrentPercent = 50;
        vm.ConfigVm.StandardAssistLevels[1].MaxCadencePercent = 100;
        vm.ConfigVm.StandardAssistLevels[1].MaxSpeedPercent = 100;
        vm.AssistLevelsVm.SelectedAssistLevel = vm.ConfigVm.StandardAssistLevels[1];
        vm.ConfigVm.CurrentRampAmpsSecond = 10;
        vm.ConfigVm.CurrentRampSmoothnessPercent = 65;
        vm.ConfigVm.PasStopDelayMilliseconds = 100;
        vm.ConfigVm.ShiftInterruptDuration = 100;
        vm.ConfigVm.PasKeepCurrentPercent = 60;
        vm.ConfigVm.PasKeepCurrentCadenceRpm = 40;
        vm.ConfigVm.ThrottleStartVoltageMillivolts = 1000;
        vm.ConfigVm.ThrottleEndVoltageMillivolts = 4000;
        var adaptiveLevel = vm.ConfigVm.StandardAssistLevels[1];
        adaptiveLevel.SelectedPasVariant = adaptiveLevel.AssistPasVariantOptions.First(v => v.Value == AssistLevelViewModel.AssistPasVariant.AdaptiveEfficiency);
        adaptiveLevel.PasBudgetWatts = 250;
        var tabs = (TabControl)window.FindName("WorkspaceTabs");
        // Hosted Windows desktops cap top-level HWNDs near 1024x768. Exercise
        // the actual WPF content offscreen at exact logical viewport sizes.
        var surface = (FrameworkElement)window.Content;
        window.Content = null;
        surface.DataContext = vm;
        surface.Resources = app.Resources;
        surface.SetResourceReference(TextElement.ForegroundProperty, "TextBrush");
        surface.SetValue(TextElement.FontFamilyProperty, window.FontFamily);
        surface.SetValue(TextElement.FontSizeProperty, window.FontSize);
        void Layout(Size size)
        {
            surface.Measure(size);
            surface.Arrange(new Rect(size));
            surface.UpdateLayout();
        }
        try
        {
            foreach (bool dark in new[] { true, false })
            {
                theme.GetMethod("Apply", BindingFlags.Static | BindingFlags.Public).Invoke(null, new object[] { dark, false });
                foreach (var size in new[] { new Size(1000, 650), new Size(1280, 720), new Size(1920, 1080) })
                {
                    Layout(size);
                    for (int index = 0; index < tabs.Items.Count; ++index)
                    {
                        tabs.SelectedIndex = index; Layout(size); Pump(); Layout(size);
                        foreach (var plot in Descendants(surface).OfType<ResponseChart>()) plot.InvalidateVisual();
                        var presenter = (FrameworkElement)tabs.Template.FindName("PART_SelectedContentHost", tabs);
                        Check(presenter.ActualWidth > tabs.ActualWidth - 220, $"{size}/{dark}/tab {index}: content uses workspace width");
                        var descendants = Descendants(presenter).OfType<FrameworkElement>().ToArray();
                        foreach (var panel in descendants.OfType<AdaptiveCardPanel>())
                        {
                            var cards = panel.Children.OfType<FrameworkElement>().Where(c => c.Visibility == Visibility.Visible).ToArray();
                            var rectangles = cards.Select(c => Bounds(c, panel)).ToArray();
                            Check(rectangles.All(r => r.Left >= 0 && r.Right <= panel.ActualWidth + 1), "cards remain within the viewport");
                            Check(!rectangles.Where((a, i) => rectangles.Skip(i + 1).Any(a.IntersectsWith)).Any(), "cards do not overlap");
                            var viewport = descendants.OfType<ScrollViewer>().First(s => s.Content == panel);
                            Check(viewport.ComputedHorizontalScrollBarVisibility != Visibility.Visible, "system has no horizontal scrollbar");
                            if (size.Width == 1920)
                                Check(viewport.ExtentHeight <= viewport.ViewportHeight + 1, $"wide cards fit: extent {viewport.ExtentHeight:0}, viewport {viewport.ViewportHeight:0}, width {panel.ActualWidth:0}");
                        }
                        if (index == 0)
                        {
                            var header = descendants.OfType<TextBlock>().First(t => t.Text == "Available controllers");
                            var refresh = descendants.OfType<Button>().First(b => AutomationProperties.GetAutomationId(b) == "RefreshAdapters");
                            Check(!Bounds(header, surface).IntersectsWith(Bounds(refresh, surface)), "connection heading does not overlap Refresh");
                            var expectedCommands = new[] { "OpenConfigCommand", "SaveConfigCommand", "SaveLogCommand", "ReadFlashCommand", "WriteFlashCommand", "ResetFlashCommand" };
                            var actions = descendants.OfType<Button>().Where(b => expectedCommands.Contains(
                                BindingOperations.GetBinding(b, Button.CommandProperty)?.Path?.Path)).ToArray();
                            Check(actions.Length == 6 && actions.All(b => b.Command != null), "Connection exposes all six bound quick actions");
                            Check(actions.All(b => Descendants(b).OfType<TextBlock>().Any(t => t.FontFamily.Source == "Segoe MDL2 Assets")),
                                "every quick action includes an icon");
                            Check(actions.Where(b => BindingOperations.GetBinding(b, Button.CommandProperty)?.Path?.Path is "ReadFlashCommand" or "WriteFlashCommand" or "ResetFlashCommand").All(b => !b.IsEnabled),
                                "controller quick actions are disabled while disconnected");
                            var write = actions.First(b => AutomationProperties.GetAutomationId(b) == "WriteConfiguration");
                            Check((write.Background as SolidColorBrush)?.Color == (app.FindResource("HoverBrush") as SolidColorBrush)?.Color,
                                "disabled Write uses a neutral background instead of the active accent");
                            Check(actions.All(b => Bounds(b, surface).Left >= 0 && Bounds(b, surface).Right <= size.Width + 1),
                                "quick actions stay within the window");
                            Check(actions.All(b => Bounds((FrameworkElement)b.Content, b).Width <= b.ActualWidth - b.Padding.Left - b.Padding.Right + 1),
                                "quick action icons and labels fit inside buttons");
                        }
                        if (index == 3)
                        {
                            var budget = descendants.OfType<TextBox>().First(b => AutomationProperties.GetAutomationId(b) == "PasBudget");
                            Check(budget.IsVisible || budget.Visibility == Visibility.Visible, "Adaptive Efficiency exposes a per-level watt budget");
                            var warning = descendants.OfType<TextBlock>().First(c => AutomationProperties.GetAutomationId(c) == "AdaptiveQuantizationWarning");
                            adaptiveLevel.PasBudgetWatts = 10; Pump();
                            Check(warning.Text.Contains("zero current") && warning.Text.Contains("throttle"), "Adaptive PAS shows quantization and throttle warnings");
                            adaptiveLevel.PasBudgetWatts = 250;
                            var preview = descendants.OfType<ResponseChart>().First(c => c.Mode == "Adaptive");
                            Check(preview.BudgetPower == adaptiveLevel.PasBudgetWatts, "adaptive preview binds to the selected level budget");
                            adaptiveLevel.PasBudgetWatts = 350; Pump();
                            Check(preview.BudgetPower == 350, "adaptive watt edits update the native preview");
                            adaptiveLevel.PasBudgetWatts = 250;
                        }
                        if (index == 2)
                        {
                            var profile = descendants.OfType<ComboBox>().First(c => AutomationProperties.GetAutomationId(c) == "EfficiencyProfile");
                            Check(profile.Items.Count == 3 && profile.Items.Cast<object>().Select(x => x.ToString()).SequenceEqual(new[] { "BBS02B", "BBSHD", "TSDZ2" }), "System exposes exactly three efficiency profiles");
                            var comfort = descendants.OfType<CheckBox>().First(c => AutomationProperties.GetAutomationId(c) == "AdaptiveComfort");
                            Check(comfort.IsChecked == false, "optional Adaptive comfort defaults OFF");
                            var label = descendants.OfType<TextBlock>().First(t => t.Text == "Max Current (A):");
                            Check((label.Foreground as SolidColorBrush)?.Color == (app.FindResource("TextBrush") as SolidColorBrush)?.Color,
                                $"form text uses the theme color: {label.Foreground} / {app.FindResource("TextBrush")}");
                            var limit = descendants.OfType<TextBox>().First(b =>
                                System.Windows.Data.BindingOperations.GetBinding(b, TextBox.TextProperty)?.Path?.Path == "ConfigVm.ThrottleGlobalSpeedLimitPercent");
                            vm.ConfigVm.ThrottleGlobalSpeedLimit = ConfigurationViewModel.ThrottleGlobalSpeedLimitOptions[0];
                            Pump();
                            Check(!limit.IsEnabled, "disabled global throttle limit disables its percentage input");
                            vm.ConfigVm.ThrottleGlobalSpeedLimit = ConfigurationViewModel.ThrottleGlobalSpeedLimitOptions[1];
                            Pump();
                            Check(limit.IsEnabled, "enabling global throttle limit enables its percentage input");
                            vm.ConfigVm.ThrottleGlobalSpeedLimit = ConfigurationViewModel.ThrottleGlobalSpeedLimitOptions[0];
                            var chart = descendants.OfType<ResponseChart>().First(c => c.Mode == "Ramp");
                            chart.SetCurrentValue(ResponseChart.SmoothnessProperty, 75u);
                            chart.SetCurrentValue(ResponseChart.RampRateProperty, 20u);
                            Pump();
                            Check(vm.ConfigVm.CurrentRampSmoothnessPercent == 75 && vm.ConfigVm.CurrentRampAmpsSecond == 20,
                                "curve edits update the configuration through existing bindings");
                            vm.ConfigVm.UseVoltageCompensation = true;
                            Check(vm.ConfigVm.GetConfig().UseVoltageCompensation, "system toggle changes the global configuration");
                        }
                        Screenshot(surface, Path.Combine(output, $"{(dark ? "dark" : "light")}-{(int)size.Width}-{index}.png"));
                    }
                }
            }
            // Exercise the real UI command handlers with rejection injected; never open a serial port.
            vm.ConfigVm.GetConfig().Validate();
            var connected = typeof(ConnectionViewModel).GetMethod("OnConnected", BindingFlags.Instance | BindingFlags.NonPublic);
            var disconnected = typeof(ConnectionViewModel).GetMethod("OnDisconnected", BindingFlags.Instance | BindingFlags.NonPublic);
            // Controller-write guards must be exercised with a matching profile;
            // an intentional hardware mismatch is covered by portable validation tests.
            vm.ConfigVm.EfficiencyProfile = BBSFW.Model.Configuration.MotorEfficiencyProfile.BBSHD;
            void Connect() => connected.Invoke(vm.ConnectionVm, new object[] { BBSFW.Model.BbsfwConnection.Controller.BBSHD, "2.0.0", BBSFW.Model.Configuration.CurrentVersion });
            Connect();
            tabs.SelectedIndex = 0; Layout(new Size(1000, 650)); Pump();
            var buttons = Descendants(surface).OfType<Button>().ToArray();
            var writeButton = buttons.First(b => AutomationProperties.GetAutomationId(b) == "WriteConfiguration");
            var resetButton = buttons.First(b => AutomationProperties.GetAutomationId(b) == "ResetConfiguration");
            Check(writeButton.IsEnabled && resetButton.IsEnabled, "controller actions enable after connecting");
            writeButton.Command.Execute(null);
            vm.WriteFlashCommand.Execute(null);
            resetButton.Command.Execute(null);
            vm.ResetFlashCommand.Execute(null);
            vm.CalibrationVm.MeasuredBatteryVolts = 50;
            vm.CalibrationVm.SaveVoltageCommand.Execute(null);
            vm.CalibrationVm.ResetVoltageCommand.Execute(null);
            Check(confirmations == 6 && !vm.ControllerOperations.IsBusy, "buttons, shared menu commands and both calibration writes all ask and honor rejection");
            long previousSession = vm.ConnectionVm.SessionId;
            approveWrites = true;
            onConfirmation = () => { disconnected.Invoke(vm.ConnectionVm, null); Connect(); };
            vm.WriteFlashCommand.Execute(null);
            Check(confirmations == 7 && vm.ConnectionVm.SessionId != previousSession && !vm.ControllerOperations.IsBusy,
                "the real connection lifecycle invalidates approved writes after reconnecting in the dialog");
            disconnected.Invoke(vm.ConnectionVm, null);
            vm.WriteFlashCommand.Execute(null);
            Check(confirmations == 7 && !vm.CanUseController, "disconnected Write cannot prompt or send");
            Check(!vm.VirtualDisplayVm.ControlsEnabled, "layout checks never arm bench controls");
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        finally { window.Close(); app.Shutdown(); }
    }
}
