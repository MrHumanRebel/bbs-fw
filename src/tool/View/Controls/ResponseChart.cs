using System;
using System.Globalization;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using BBSFW.Model;

namespace BBSFW.View.Controls
{
    /// <summary>Small native chart; no charting library, timer or hardware communication.</summary>
    public sealed class ResponseChart : FrameworkElement
    {
        private static DependencyProperty ValueProperty(string name, Type type, object value, bool editable = false) =>
            DependencyProperty.Register(name, type, typeof(ResponseChart), new FrameworkPropertyMetadata(value,
                FrameworkPropertyMetadataOptions.AffectsRender | (editable ? FrameworkPropertyMetadataOptions.BindsTwoWayByDefault : 0)));
        public static readonly DependencyProperty ModeProperty = ValueProperty(nameof(Mode), typeof(string), "Ramp");
        public static readonly DependencyProperty RampRateProperty = ValueProperty(nameof(RampRate), typeof(uint), 10u, true);
        public static readonly DependencyProperty SmoothnessProperty = ValueProperty(nameof(Smoothness), typeof(uint), 0u, true);
        public static readonly DependencyProperty MaxCurrentProperty = ValueProperty(nameof(MaxCurrent), typeof(uint), 25u);
        public static readonly DependencyProperty StartProperty = ValueProperty(nameof(Start), typeof(double), 1000d);
        public static readonly DependencyProperty EndProperty = ValueProperty(nameof(End), typeof(double), 4000d);
        public static readonly DependencyProperty KeepProperty = ValueProperty(nameof(Keep), typeof(uint), 60u);
        public static readonly DependencyProperty CadenceProperty = ValueProperty(nameof(Cadence), typeof(uint), 40u);
        public string Mode { get => (string)GetValue(ModeProperty); set => SetValue(ModeProperty, value); }
        public uint RampRate { get => (uint)GetValue(RampRateProperty); set => SetValue(RampRateProperty, value); }
        public uint Smoothness { get => (uint)GetValue(SmoothnessProperty); set => SetValue(SmoothnessProperty, value); }
        public uint MaxCurrent { get => (uint)GetValue(MaxCurrentProperty); set => SetValue(MaxCurrentProperty, value); }
        public double Start { get => (double)GetValue(StartProperty); set => SetValue(StartProperty, value); }
        public double End { get => (double)GetValue(EndProperty); set => SetValue(EndProperty, value); }
        public uint Keep { get => (uint)GetValue(KeepProperty); set => SetValue(KeepProperty, value); }
        public uint Cadence { get => (uint)GetValue(CadenceProperty); set => SetValue(CadenceProperty, value); }
        private bool draggingShape;
        private double dragTimeScale;
        private Rect Plot => new Rect(38, 22, Math.Max(1, ActualWidth - 54), Math.Max(1, ActualHeight - 56));
        private double TimeScale => IsMouseCaptured ? dragTimeScale : Math.Max(0.05, ResponseMath.RiseSeconds(MaxCurrent, RampRate) * 1.4);
        public ResponseChart()
        {
            Height = 168;
            Focusable = true;
            Loaded += (_, _) => BBSFW.Themes.ThemeManager.Changed += RefreshTheme;
            Unloaded += (_, _) => BBSFW.Themes.ThemeManager.Changed -= RefreshTheme;
            ToolTip = "Ramp: drag the duration handle horizontally; drag the shape handle vertically. Arrow keys adjust rate and softness. Numeric controls offer the same adjustments.";
        }
        private void RefreshTheme() => InvalidateVisual();
        private Brush Brush(string name) => TryFindResource(name) as Brush ?? Brushes.Gray;
        private void Label(DrawingContext dc, string text, Point p, Brush color)
        {
            var formatted = new FormattedText(text, CultureInfo.CurrentCulture, FlowDirection.LeftToRight,
                new Typeface("Segoe UI"), 10, color, VisualTreeHelper.GetDpi(this).PixelsPerDip);
            dc.DrawText(formatted, p);
        }
        private double Evaluate(double x)
        {
            if (Mode == "Ramp") return ResponseMath.Ramp((int)Math.Min(1000, x / Math.Max(0.001, ResponseMath.RiseSeconds(MaxCurrent, RampRate)) * 1000), Smoothness) / 1000d;
            if (Mode == "Throttle") return End <= Start ? 0 : x < Start ? 0 : .01 + .99 * Math.Clamp((x - Start) / (End - Start), 0, 1);
            // Reference cadence uses a 100% cadence limit (150 rpm), as in app.c.
            return 1 - Math.Clamp((x - Cadence) / Math.Max(1, 150d - Cadence), 0, 1) * (1 - Keep / 100d);
        }
        protected override void OnRender(DrawingContext dc)
        {
            var r = Plot;
            var muted = Brush("MutedBrush");
            var accent = Brush("AccentBrush");
            dc.DrawRectangle(Brushes.Transparent, null, new Rect(RenderSize));
            if ((Mode == "Ramp" && (MaxCurrent == 0 || RampRate == 0)) || (Mode == "Throttle" && End <= Start))
            {
                Label(dc, Mode == "Ramp" ? "Set Max Current and ramp rate to preview." : "Set valid start/end voltages to preview.", new Point(8, 60), muted);
                return;
            }
            var grid = new Pen(Brush("BorderBrush"), 0.5);
            double maxX = Mode == "Ramp" ? TimeScale : Mode == "Throttle" ? 5000 : 160;
            for (int i = 0; i <= 4; ++i)
            {
                double y = r.Bottom - r.Height * i / 4;
                dc.DrawLine(grid, new Point(r.Left, y), new Point(r.Right, y));
                Label(dc, $"{i * 25}%", new Point(0, y - 6), muted);
            }
            var geometry = new StreamGeometry();
            using (var context = geometry.Open())
            {
                context.BeginFigure(new Point(r.Left, r.Bottom - Evaluate(0) * r.Height), false, false);
                for (int i = 1; i <= 100; ++i) context.LineTo(new Point(r.Left + r.Width * i / 100,
                    r.Bottom - Evaluate(maxX * i / 100) * r.Height), true, false);
            }
            geometry.Freeze();
            dc.DrawGeometry(null, new Pen(accent, 2.5), geometry);
            Label(dc, "0", new Point(r.Left, r.Bottom + 9), muted);
            Label(dc, Mode == "Ramp" ? $"{maxX:0.##} s" : Mode == "Throttle" ? "5000 mV" : "160 rpm", new Point(r.Right - 50, r.Bottom + 9), muted);
            if (Mode == "Ramp")
            {
                double duration = ResponseMath.RiseSeconds(MaxCurrent, RampRate);
                var finish = new Point(r.Left + Math.Min(1, duration / maxX) * r.Width, r.Top);
                var shape = new Point(r.Left + duration * .25 / maxX * r.Width,
                    r.Bottom - ResponseMath.Ramp(250, Smoothness) / 1000d * r.Height);
                dc.DrawEllipse(Brush("SurfaceBrush"), new Pen(accent, 2), finish, 6, 6);
                dc.DrawEllipse(Brush("SurfaceBrush"), new Pen(accent, 2), shape, 6, 6);
                Label(dc, "Duration", new Point(finish.X - 35, 0), muted);
                Label(dc, "Shape", new Point(shape.X + 10, shape.Y - 5), muted);
            }
        }
        protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
        {
            base.OnMouseLeftButtonDown(e);
            if (Mode != "Ramp" || MaxCurrent == 0) return;
            Focus();
            dragTimeScale = TimeScale;
            draggingShape = e.GetPosition(this).X < Plot.Left + Plot.Width * 0.45;
            CaptureMouse();
            Edit(e.GetPosition(this));
            e.Handled = true;
        }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            if (IsMouseCaptured && e.LeftButton == MouseButtonState.Pressed) Edit(e.GetPosition(this));
        }
        private void Edit(Point point)
        {
            var r = Plot;
            if (draggingShape)
            {
                double value = (r.Bottom - point.Y) / r.Height * 1000;
                SetCurrentValue(SmoothnessProperty, (uint)Math.Clamp(Math.Round((250 - value) * 100 / 94), 0, 100));
            }
            else
            {
                double duration = Math.Max(0.001, Math.Clamp((point.X - r.Left) / r.Width, 0, 1) * dragTimeScale);
                SetCurrentValue(RampRateProperty, (uint)Math.Clamp(Math.Round(MaxCurrent / duration), 1, 255));
            }
        }
        protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
        {
            base.OnMouseLeftButtonUp(e);
            if (IsMouseCaptured) { ReleaseMouseCapture(); InvalidateVisual(); e.Handled = true; }
        }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);
            if (Mode != "Ramp" || MaxCurrent == 0) return;
            if (e.Key == Key.Left || e.Key == Key.Right) SetCurrentValue(RampRateProperty, (uint)Math.Clamp((int)RampRate + (e.Key == Key.Right ? 1 : -1), 1, 255));
            else if (e.Key == Key.Up || e.Key == Key.Down) SetCurrentValue(SmoothnessProperty, (uint)Math.Clamp((int)Smoothness + (e.Key == Key.Up ? 1 : -1), 0, 100));
            else return;
            e.Handled = true;
        }
    }
}
