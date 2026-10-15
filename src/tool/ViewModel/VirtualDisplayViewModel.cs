using BBSFW.Model;
using BBSFW.ViewModel.Base;
using System;
using System.Threading.Tasks;
using System.Windows.Threading;

namespace BBSFW.ViewModel
{
    public class VirtualDisplayViewModel : ObservableObject
    {
        private readonly ConnectionViewModel _connection;
        private readonly ConfigurationViewModel _config;
        private readonly DispatcherTimer _timer;
        private bool _busy, _enabled, _armed, _ownsControl, _holding;
        private VirtualTelemetry _telemetry;
        private string _message = "Connect to the updated firmware to read live data.";
        private int _level;
        public int[] Levels { get; } = {0,1,2,3,4,5,6,7,8,9};
        public int SelectedLevel { get => _level; set { _level = value; OnPropertyChanged(nameof(SelectedLevel)); } }
        private double _throttleDemand;
        public double ThrottleDemand
        {
            get => _throttleDemand;
            set { _throttleDemand = Math.Clamp(value, 0, 100); OnPropertyChanged(nameof(ThrottleDemand)); }
        }
        public bool Supported { get; private set; }
        public string Message => _message;
        public string Speed => _telemetry == null ? "—" : string.Format("{0:F1} {1}",
            _telemetry.SpeedKph * (_config.UseImperialUnits ? 0.621371192 : 1), _config.UseImperialUnits ? "mph" : "km/h");
        public string Power => _telemetry == null ? "—" : string.Format("{0:F0} W", _telemetry.Watts);
        public string Electrical => _telemetry == null ? "—" : string.Format("{0:F1} V   {1:F1} A   PAS {2}   Status 0x{3:X2}",
            _telemetry.Volts, _telemetry.Amps, _telemetry.AssistLevel, _telemetry.Status);
        public bool ControlsEnabled
        {
            get => _enabled;
            set
            {
                if (!value) { _ = Stop(); return; }
                if (!Supported || !_connection.IsConnected) return;
                _enabled = true; _armed = false; _ownsControl = true; _holding = false;
                SelectedLevel = 0; ThrottleDemand = 0;
                OnPropertyChanged(nameof(ControlsEnabled));
            }
        }

        public VirtualDisplayViewModel(ConnectionViewModel connection, ConfigurationViewModel config)
        {
            _connection = connection; _config = config;
            _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
            _timer.Tick += async (_, __) => await Poll();
            connection.PropertyChanged += (_, e) => {
                if (e.PropertyName == nameof(connection.IsConnected))
                    _timer.Dispatcher.BeginInvoke(new Action(async () => {
                        if (!connection.IsConnected) { await Stop(); _telemetry = null; Supported = false; Notify(); }
                    }));
            };
        }
        public void Start() { _timer.Start(); _ = Poll(); }
        public void Hold(bool held)
        {
            _holding = held && _enabled;
            if (!held && _enabled)
            {
                try { _connection.GetConnection().ReleaseVirtualControl(false, (byte)SelectedLevel); }
                catch (Exception) { _ = Stop(); }
            }
        }
        public async Task Leave() { _timer.Stop(); await Stop(); }
        public Task Stop()
        {
            bool owned = _ownsControl;
            _enabled = _armed = _ownsControl = _holding = false;
            OnPropertyChanged(nameof(ControlsEnabled));
            if (owned && _connection.IsConnected)
            {
                try { _connection.GetConnection().ReleaseVirtualControl(true, 0); }
                catch (Exception) { /* Firmware lease also expires independently. */ }
            }
            return Task.CompletedTask;
        }
        private async Task Poll()
        {
            if (_busy || !_connection.IsConnected) return;
            _busy = true;
            try
            {
                if (_enabled)
                {
                    byte demand = (byte)(_armed && _holding ? Math.Clamp(ThrottleDemand, 0, 100) : 0);
                    bool accepted = await _connection.GetConnection().SendVirtualControl(true, (byte)SelectedLevel, demand);
                    if (!accepted) { await Stop(); _message = "Control rejected or timed out; controls disarmed."; Notify(); return; }
                    _armed = _enabled;
                }
                var data = await _connection.GetConnection().ReadVirtualTelemetry();
                if (data == null)
                {
                    await Stop(); Supported = false; _telemetry = null;
                    _message = "No compatible telemetry response. Controls are disabled.";
                }
                else
                {
                    _telemetry = data; Supported = true;
                    _message = _enabled ? "Bench control active. Hold the throttle button to apply the slider value." : "Live data; bench controls are disabled.";
                }
                Notify();
            }
            catch (Exception ex) { await Stop(); Supported = false; _telemetry = null; _message = ex.Message; Notify(); }
            finally { _busy = false; }
        }
        private void Notify()
        {
            foreach (string property in new[] {nameof(Speed),nameof(Power),nameof(Electrical),nameof(Message),nameof(Supported)})
                OnPropertyChanged(property);
        }
    }
}
