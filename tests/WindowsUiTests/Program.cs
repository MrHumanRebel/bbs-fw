using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
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
        var vm = (MainViewModel)window.DataContext;
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
        vm.ConfigVm.PasKeepCurrentPercent = 60;
        vm.ConfigVm.PasKeepCurrentCadenceRpm = 40;
        vm.ConfigVm.ThrottleStartVoltageMillivolts = 1000;
        vm.ConfigVm.ThrottleEndVoltageMillivolts = 4000;
        var tabs = (TabControl)window.FindName("WorkspaceTabs");
        // Hosted Windows desktops cap top-level HWNDs near 1024x768. Exercise
        // the actual WPF content offscreen at exact logical viewport sizes.
        var surface = (FrameworkElement)window.Content;
        window.Content = null;
        surface.DataContext = vm;
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
                            var refresh = descendants.OfType<Button>().First(b => Equals(b.Content, "Refresh"));
                            Check(!Bounds(header, surface).IntersectsWith(Bounds(refresh, surface)), "connection heading does not overlap Refresh");
                        }
                        if (index == 2)
                        {
                            var label = descendants.OfType<TextBlock>().First(t => t.Text == "Max Current (A):");
                            Check(label.Foreground == app.FindResource("TextBrush"), "navigation accent does not leak into form text");
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
            Check(!vm.VirtualDisplayVm.ControlsEnabled, "layout checks never arm bench controls");
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        finally { window.Close(); app.Shutdown(); }
    }
}
