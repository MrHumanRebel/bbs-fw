using BBSFW.Model;
using BBSFW.ViewModel.Base;
using Microsoft.Win32;
using System;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;

namespace BBSFW.ViewModel
{

	public class MainViewModel : ObservableObject
	{

		public ConfigurationViewModel ConfigVm { get; private set; }

		public ConnectionViewModel ConnectionVm { get; private set; }

		public SystemViewModel SystemVm { get; private set; }

		public AssistLevelsViewModel AssistLevelsVm { get; private set; }

		public CalibrationViewModel CalibrationVm { get; private set; }

		public EventLogViewModel EventLogVm { get; private set; }

        public VirtualDisplayViewModel VirtualDisplayVm { get; private set; }

        public ControllerOperationGuard ControllerOperations { get; }
        public bool CanUseController => ConnectionVm.IsConnected && !ControllerOperations.IsBusy;
        public bool CanUseFiles => !ControllerOperations.IsBusy;



		public ICommand OpenConfigCommand
		{
			get { return new DelegateCommand(OnOpenConfig); }
		}

		public ICommand SaveConfigCommand
		{
			get { return new DelegateCommand(OnSaveConfig); }
		}

		public ICommand SaveLogCommand
		{
			get { return new DelegateCommand(OnSaveLog); }
		}

		public ICommand ReadFlashCommand
		{
			get { return new DelegateCommand(OnReadFlash); }
		}

		public ICommand WriteFlashCommand
		{
			get { return new DelegateCommand(OnWriteFlash); }
		}

		public ICommand ResetFlashCommand
		{
			get { return new DelegateCommand(OnResetFlash); }
		}

		public ICommand ExitCommand
		{
			get { return new DelegateCommand(OnExit); }
		}

		public ICommand ShowAboutCommand
		{
			get { return new DelegateCommand(OnShowAbout); }
		}



		public MainViewModel() : this(ControllerConfirmation.Ask) { }

        public MainViewModel(Func<string, string, bool> confirmControllerWrite)
        {
			ConfigVm = new ConfigurationViewModel();

			ConnectionVm = new ConnectionViewModel();
			SystemVm = new SystemViewModel(ConfigVm);
			AssistLevelsVm = new AssistLevelsViewModel(ConfigVm);
			EventLogVm = new EventLogViewModel();
            VirtualDisplayVm = new VirtualDisplayViewModel(ConnectionVm, ConfigVm);
            ControllerOperations = new ControllerOperationGuard(confirmControllerWrite);
            CalibrationVm = new CalibrationViewModel(ConnectionVm, ControllerOperations, VirtualDisplayVm.Stop);
            ConnectionVm.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName == nameof(ConnectionVm.IsConnected)) OnPropertyChanged(nameof(CanUseController));
            };
            ControllerOperations.PropertyChanged += (_, e) =>
            {
                OnPropertyChanged(nameof(CanUseController));
                OnPropertyChanged(nameof(CanUseFiles));
            };


			ConnectionVm.EventLogReceived += EventLogVm.AddEvent;
		}


		private void OnSaveLog()
		{
			var dialog = new SaveFileDialog();

			dialog.Filter = "Log File|*.log";
			dialog.Title = "Save Log";
			dialog.FileName = "bbsfw.log";

			var result = dialog.ShowDialog();
			if (result.HasValue && result.Value)
			{
				try
				{
					EventLogVm.ExportLog(dialog.FileName);
				}
				catch(Exception e)
				{
					MessageBox.Show(e.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				}
			}
		}

		private void OnOpenConfig()
		{
            if (!CanUseFiles) return;
			var dialog = new OpenFileDialog();
			dialog.Filter = "XML File|*.xml";
			dialog.Title = "Open Configuration";

			var result = dialog.ShowDialog();
			if (result.HasValue && result.Value)
			{
				try
				{
					ConfigVm.ReadConfiguration(dialog.FileName);
				}
				catch (Exception e)
				{
					MessageBox.Show(e.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				}
			}
		}

		private void OnSaveConfig()
		{
            if (!CanUseFiles) return;
			if (!ValidateConfig())
			{
				return;
			}

			var dialog = new SaveFileDialog();

			dialog.Filter = "XML File|*.xml";
			dialog.Title = "Save Configuration";
			dialog.FileName = "bbsfw.xml";

			var result = dialog.ShowDialog();
			if (result.HasValue && result.Value)
			{
				try
				{
					ConfigVm.WriteConfiguration(dialog.FileName);
				}
				catch (Exception e)
				{
					MessageBox.Show(e.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				}
			}
		}

        private async void OnReadFlash()
        {
            if (!CanUseController) return;
            await VirtualDisplayVm.Stop();
            if (!VerifyConfigVersionForRead()) return;
            long session = ConnectionVm.SessionId;
            try
            {
                await ControllerOperations.RunRead(
                    () => ConnectionVm.IsConnected && ConnectionVm.SessionId == session,
                    () => ReadConfiguration(session));
            }
            catch (Exception e) { ShowControllerError(e); }
        }

        private async Task ReadConfiguration(long session)
        {
            var res = await ConnectionVm.GetConnection().ReadConfiguration(TimeSpan.FromSeconds(5));
            if (!ConnectionVm.IsConnected || ConnectionVm.SessionId != session) return;
            if (!res.Timeout && res.Result != null)
                ConfigVm.UpdateFrom(res.Result);
            else
                MessageBox.Show("Failed to read configuration from flash, timeout occured.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        }

        private async void OnWriteFlash()
        {
            if (!CanUseController) return;
            await VirtualDisplayVm.Stop();
            if (!ValidateConfig() || !VerifyConfigVersionForWrite()) return;

            // Write exactly the reviewed values, even if a setting changes while awaiting the response.
            var configuration = new Configuration();
            configuration.CopyFrom(ConfigVm.GetConfig());
            long session = ConnectionVm.SessionId;
            string message = $"Write configuration to {ConnectionVm.Controller} (firmware {ConnectionVm.FirmwareVersion})?\n\n" +
                $"Maximum current: {configuration.MaxCurrentAmps} A\n" +
                $"Battery: {configuration.LowCutoffVolts}–{configuration.MaxBatteryVolts:0.##} V\n\n" +
                "This replaces the settings stored in the controller. Keep the driven wheel clear. " +
                "Restart the controller before using the new settings.";
            try
            {
                await ControllerOperations.RunWrite("Write controller configuration?", message,
                    () => ConnectionVm.IsConnected && ConnectionVm.SessionId == session,
                    async () =>
                    {
                        var res = await ConnectionVm.GetConnection().WriteConfiguration(configuration, TimeSpan.FromSeconds(5));
                        if (res.Timeout)
                            MessageBox.Show("Failed to write configuration to flash, timeout occured.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        else if (!res.Result)
                            MessageBox.Show("Failed to write configuration to flash, try again.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        else
                            MessageBox.Show("Configuration written. Restart the controller before using the new settings.", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                    });
            }
            catch (Exception e) { ShowControllerError(e); }
        }

        private async void OnResetFlash()
        {
            if (!CanUseController) return;
            await VirtualDisplayVm.Stop();
            long session = ConnectionVm.SessionId;
            try
            {
                await ControllerOperations.RunWrite("Reset controller configuration?",
                    $"Restore firmware configuration defaults on {ConnectionVm.Controller} (firmware {ConnectionVm.FirmwareVersion})?\n\n" +
                    "This overwrites the controller settings. Save a backup first. Keep the driven wheel clear. " +
                    "Restart the controller before using the reset settings.",
                    () => ConnectionVm.IsConnected && ConnectionVm.SessionId == session,
                    async () =>
                    {
                        var res = await ConnectionVm.GetConnection().ResetConfiguration(TimeSpan.FromSeconds(5));
                        if (res.Timeout)
                            MessageBox.Show("Failed to reset configuration, timeout occured.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        else if (!res.Result)
                            MessageBox.Show("Failed to reset configuration, try again.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        else if (ConnectionVm.IsConnected && ConnectionVm.SessionId == session && VerifyConfigVersionForRead())
                            await ReadConfiguration(session);
                    });
            }
            catch (Exception e) { ShowControllerError(e); }
        }

        private static void ShowControllerError(Exception error) =>
            MessageBox.Show(error.Message, "Controller operation failed", MessageBoxButton.OK, MessageBoxImage.Error);

		private void OnShowAbout()
		{
			var version = Assembly.GetExecutingAssembly().GetName().Version;
			MessageBox.Show($"Version: {version.Major}.{version.Minor}.{version.Build}\nAuthor: Daniel Nilsson", "BBS-FW Tool", MessageBoxButton.OK, MessageBoxImage.Information);
		}

		private async void OnExit()
		{
            await VirtualDisplayVm.Stop();
			Application.Current.Shutdown();
		}

		private bool VerifyConfigVersionForRead()
		{
			if (ConnectionVm.ConfigVersion < Configuration.MinVersion || ConnectionVm.ConfigVersion > Configuration.MaxVersion)
			{
				MessageBox.Show("Unsupported firmware config version. Please use BBS-FW Config Tool for firmware version " + ConnectionVm.FirmwareVersion + " to read configuration from flash.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				return false;
			}

			return true;
		}

		private bool VerifyConfigVersionForWrite()
		{
			if (ConnectionVm.ConfigVersion != Configuration.CurrentVersion)
			{
				MessageBox.Show("Unsupported firmware config version. Please use BBS-FW Config Tool for firmware version " + ConnectionVm.FirmwareVersion + " in order to write configuration to flash, or upgrade firmware to latest version.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				return false;
			}

			return true;
		}

		private bool ValidateConfig()
		{
			try
			{
				ConfigVm.GetConfig().Validate();
				return true;
			}
			catch (Exception e)
			{
				MessageBox.Show(e.Message, "Validation Error", MessageBoxButton.OK, MessageBoxImage.Error);
			}

			return false;
		}

	}
}
