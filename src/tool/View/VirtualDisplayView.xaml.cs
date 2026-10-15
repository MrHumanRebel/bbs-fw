using BBSFW.ViewModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace BBSFW.View
{
    public partial class VirtualDisplayView : UserControl
    {
        private VirtualDisplayViewModel Vm => DataContext as VirtualDisplayViewModel;
        private Window _window;
        public VirtualDisplayView() { InitializeComponent(); }
        private void OnLoaded(object s, RoutedEventArgs e) { _window = Window.GetWindow(this); if (_window != null) _window.Deactivated += Deactivated; Vm?.Start(); }
        private async void OnUnloaded(object s, RoutedEventArgs e) { if (_window != null) _window.Deactivated -= Deactivated; if (Vm != null) await Vm.Leave(); }
        private async void Deactivated(object s, System.EventArgs e) { if (Vm != null) await Vm.Stop(); }
        private void ThrottleDown(object s, MouseButtonEventArgs e) { ((UIElement)s).CaptureMouse(); Vm?.Hold(true); }
        private void ThrottleUp(object s, MouseButtonEventArgs e) { Vm?.Hold(false); ((UIElement)s).ReleaseMouseCapture(); }
        private void ThrottleLost(object s, RoutedEventArgs e) { Vm?.Hold(false); }
        private void ThrottleKeyDown(object s, KeyEventArgs e) { if (e.Key == Key.Space) { Vm?.Hold(true); e.Handled = true; } }
        private void ThrottleKeyUp(object s, KeyEventArgs e) { if (e.Key == Key.Space) { Vm?.Hold(false); e.Handled = true; } }
        private async void StopClick(object s, RoutedEventArgs e) { if (Vm != null) await Vm.Stop(); }
    }
}
