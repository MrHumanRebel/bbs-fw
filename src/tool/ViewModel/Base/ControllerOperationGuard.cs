using System;
using System.Threading;
using System.Threading.Tasks;

namespace BBSFW.ViewModel.Base
{
    /// <summary>Serializes configuration/calibration requests and obtains fresh approval for every persistent write.</summary>
    public sealed class ControllerOperationGuard : ObservableObject
    {
        private readonly Func<string, string, bool> _confirm;
        private int _busy;

        public bool IsBusy => Volatile.Read(ref _busy) != 0;

        public ControllerOperationGuard(Func<string, string, bool> confirm)
        {
            _confirm = confirm ?? throw new ArgumentNullException(nameof(confirm));
        }

        public Task<bool> RunRead(Func<bool> isCurrentConnection, Func<Task> read) =>
            Run(null, isCurrentConnection, read);

        public Task<bool> RunWrite(string title, string message, Func<bool> isCurrentConnection, Func<Task> write) =>
            Run(() => _confirm(title, message), isCurrentConnection, write);

        private async Task<bool> Run(Func<bool> confirm, Func<bool> isCurrentConnection, Func<Task> operation)
        {
            // Reject overlapping clicks rather than queuing a later write with stale approval.
            if (Interlocked.CompareExchange(ref _busy, 1, 0) != 0) return false;
            try
            {
                OnPropertyChanged(nameof(IsBusy));
                if (!isCurrentConnection()) return false;
                if (confirm != null && !confirm()) return false;
                // A modal dialog pumps messages: the adapter may have disconnected or reconnected.
                if (!isCurrentConnection()) return false;
                await operation();
                return true;
            }
            finally
            {
                Volatile.Write(ref _busy, 0);
                OnPropertyChanged(nameof(IsBusy));
            }
        }
    }
}
