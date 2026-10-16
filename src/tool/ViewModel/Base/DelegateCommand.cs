using System;
using System.Windows.Input;

namespace BBSFW.ViewModel.Base
{
	public class DelegateCommand : ICommand
	{
		private readonly Action _action;
		private readonly Action<object> _actionWithParam;

		// CanExecute is constant, so there are no availability changes to notify.
        // Explicit accessors satisfy ICommand without storing unused subscribers.
        public event EventHandler CanExecuteChanged
        {
            add { }
            remove { }
        }

		public DelegateCommand(Action action)
		{
			_action = action;
			_actionWithParam = null;
		}

		public DelegateCommand(Action<object> action)
		{
			_actionWithParam = action;
			_action = null;
		}

		public bool CanExecute(object parameter)
		{
			return true;
		}

		public void Execute(object parameter)
		{
			_action?.Invoke();
			_actionWithParam?.Invoke(parameter);
		}
	}
}
