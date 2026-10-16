using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Animation;
using BBSFW.Themes;

namespace BBSFW
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            var workArea = SystemParameters.WorkArea;
            MinWidth = Math.Min(MinWidth, workArea.Width);
            MinHeight = Math.Min(MinHeight, workArea.Height);
            Width = Math.Min(Width, workArea.Width);
            Height = Math.Min(Height, workArea.Height);
            UpdateThemeLabel();
        }

        private void ToggleTheme(object sender, RoutedEventArgs e)
        {
            ThemeManager.Apply(!ThemeManager.IsDark);
            UpdateThemeLabel();
        }

        private void UpdateThemeLabel()
        {
            ThemeButton.Content = ThemeManager.IsDark ? "☀  Light mode" : "☾  Dark mode";
        }

        private void WorkspaceChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.Source != WorkspaceTabs || !SystemParameters.ClientAreaAnimation) return;
            // Animate only presentation opacity, never the input/stop handlers.
            var presenter = WorkspaceTabs.Template?.FindName("PART_SelectedContentHost", WorkspaceTabs) as FrameworkElement;
            if (presenter == null) return;
            presenter.BeginAnimation(OpacityProperty, new DoubleAnimation(0.65, 1, TimeSpan.FromMilliseconds(160))
            {
                FillBehavior = FillBehavior.Stop,
                EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut }
            });
        }
    }
}
