using BBSFW.ViewModel.Base;
using System;
using System.Windows;
using System.Threading.Tasks;
using System.Windows.Input;

namespace BBSFW.ViewModel
{
	public class CalibrationViewModel : ObservableObject
	{
		private readonly ConnectionViewModel _connectionVm;
        private readonly ControllerOperationGuard _operations;
        private readonly Func<Task> _stopControls;
        public bool CanWrite => _connectionVm.IsConnected && !_operations.IsBusy;

		private float _batteryStatusVolts;
		public float BatteryStatusVolts
		{
			get { return _batteryStatusVolts; }
			set
			{
				if (_batteryStatusVolts != value)
				{
					_batteryStatusVolts = value;
					OnPropertyChanged(nameof(BatteryStatusVolts));
				}
			}
		}

		private float _measuredBatteryVolts;
		public float MeasuredBatteryVolts
		{
			get { return _measuredBatteryVolts; }
			set
			{
				if (_measuredBatteryVolts != value)
				{
					_measuredBatteryVolts = value;
					OnPropertyChanged(nameof(MeasuredBatteryVolts));
				}
			}
		}


		public ICommand SaveVoltageCommand
		{
			get { return new DelegateCommand(OnSaveVoltageCalibration); }
		}

		public ICommand ResetVoltageCommand
		{
			get { return new DelegateCommand(OnResetVoltageCalibration); }
		}


        public CalibrationViewModel(ConnectionViewModel connectionVm, ControllerOperationGuard operations, Func<Task> stopControls)
        {
            _connectionVm = connectionVm;
            _operations = operations;
            _stopControls = stopControls;
            connectionVm.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName == nameof(connectionVm.IsConnected)) OnPropertyChanged(nameof(CanWrite));
            };
            operations.PropertyChanged += (_, e) => OnPropertyChanged(nameof(CanWrite));
        }

        private async void OnSaveVoltageCalibration()
        {
            if (!CanWrite) return;
            float voltage = MeasuredBatteryVolts;
            if (!float.IsFinite(voltage) || voltage < 1 || voltage > 100)
            {
                MessageBox.Show("Measured Battery Voltage must be in range [1, 100]", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            await WriteCalibration(voltage, false);
        }

        private async void OnResetVoltageCalibration()
        {
            if (!CanWrite) return;
            await WriteCalibration(0f, true);
        }

        private async Task WriteCalibration(float voltage, bool reset)
        {
            await _stopControls();
            long session = _connectionVm.SessionId;
            try
            {
                string details = reset ? "Restore the default battery voltage calibration" : $"Save battery voltage calibration at {voltage:0.##} V";
                await _operations.RunWrite(reset ? "Reset voltage calibration?" : "Save voltage calibration?",
                    $"{details} on {_connectionVm.Controller} (firmware {_connectionVm.FirmwareVersion})?\n\n" +
                    "This writes the controller's persistent calibration. Keep the driven wheel clear. Incorrect calibration affects low-voltage protection.",
                    () => _connectionVm.IsConnected && _connectionVm.SessionId == session,
                    async () =>
                    {
                        var res = await _connectionVm.GetConnection().CalibrateBatteryVoltage(voltage, TimeSpan.FromSeconds(3));
                        if (res.Timeout)
                            MessageBox.Show("Voltage calibration timed out. Check the controller and log before trying again.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        else if (!res.Result)
                            MessageBox.Show("Failed to update voltage calibration, check log.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        else
                            MessageBox.Show(reset ? "Voltage calibration reset!" : "Voltage calibration saved!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                    });
            }
            catch (Exception e)
            {
                MessageBox.Show(e.Message, "Controller operation failed", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}
