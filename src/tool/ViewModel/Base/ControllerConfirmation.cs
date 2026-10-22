using System.Windows;

namespace BBSFW.ViewModel.Base
{
    internal static class ControllerConfirmation
    {
        public static bool Ask(string title, string message)
        {
            var owner = Application.Current?.MainWindow;
            var result = owner != null && owner.IsVisible
                ? MessageBox.Show(owner, message, title, MessageBoxButton.YesNo,
                    MessageBoxImage.Warning, MessageBoxResult.No)
                : MessageBox.Show(message, title, MessageBoxButton.YesNo,
                    MessageBoxImage.Warning, MessageBoxResult.No);
            return result == MessageBoxResult.Yes;
        }
    }
}
