using System;
using System.Threading.Tasks;
using BBSFW.ViewModel.Base;

internal static class ControllerOperationTests
{
    public static async Task Run(Action<bool, string> check)
    {
        int confirmations = 0, writes = 0;
        bool approval = false, connected = true;
        var guard = new ControllerOperationGuard((title, message) => { ++confirmations; return approval; });
        Task Write() { ++writes; return Task.CompletedTask; }

        await guard.RunWrite("Write", "Configuration", () => connected, Write);
        await guard.RunWrite("Write", "Configuration", () => connected, Write);
        check(confirmations == 2 && writes == 0 && !guard.IsBusy, "every denied write asks again and sends nothing");
        connected = false;
        approval = true;
        await guard.RunWrite("Write", "Configuration", () => connected, Write);
        check(confirmations == 2 && writes == 0, "disconnected requests cannot prompt or write");
        connected = true;
        await guard.RunWrite("Write", "Configuration", () => connected, Write);
        await guard.RunWrite("Reset", "Defaults", () => connected, Write);
        check(confirmations == 4 && writes == 2, "approved writes and resets each require fresh confirmation");

        int session = 1;
        var reconnectGuard = new ControllerOperationGuard((_, _) => { ++session; return true; });
        await reconnectGuard.RunWrite("Write", "Configuration", () => session == 1, Write);
        check(writes == 2 && !reconnectGuard.IsBusy, "reconnecting during confirmation invalidates approval");
        var disconnectGuard = new ControllerOperationGuard((_, _) => { connected = false; return true; });
        await disconnectGuard.RunWrite("Write", "Configuration", () => connected, Write);
        check(writes == 2, "disconnecting during confirmation prevents the write");
        connected = true;

        var pending = new TaskCompletionSource();
        var first = guard.RunWrite("Write", "Configuration", () => connected, async () => { ++writes; await pending.Task; });
        check(guard.IsBusy, "controller stays busy while awaiting a write response");
        bool second = await guard.RunWrite("Write", "Configuration", () => connected, Write);
        bool read = await guard.RunRead(() => connected, () => throw new Exception("Read overlapped a write"));
        check(!second && !read && confirmations == 5 && writes == 3, "overlapping requests are rejected without queuing or reusing approval");
        pending.SetResult();
        await first;
        check(!guard.IsBusy, "completed operations release the controller guard");

        var pendingRead = new TaskCompletionSource();
        var reading = guard.RunRead(() => connected, () => pendingRead.Task);
        second = await guard.RunWrite("Write", "Configuration", () => connected, Write);
        check(!second && confirmations == 5 && writes == 3, "a pending read also blocks writes without confirmation");
        pendingRead.SetResult();
        await reading;

        try
        {
            await guard.RunWrite("Write", "Configuration", () => connected, () => throw new InvalidOperationException("Adapter removed"));
            throw new Exception("Write exception was swallowed");
        }
        catch (InvalidOperationException) { }
        check(!guard.IsBusy && confirmations == 6 && writes == 3, "write failures release the guard without an automatic retry");
        approval = false;
        await guard.RunWrite("Write", "Configuration", () => connected, Write);
        check(confirmations == 7 && writes == 3, "retry after failure needs new approval");

        var failingDialog = new ControllerOperationGuard((_, _) => throw new InvalidOperationException("Dialog unavailable"));
        try { await failingDialog.RunWrite("Write", "Configuration", () => true, Write); }
        catch (InvalidOperationException) { }
        check(!failingDialog.IsBusy && writes == 3, "confirmation failures never write and release the guard");
    }
}
