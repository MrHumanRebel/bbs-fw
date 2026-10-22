using System;

namespace BBSFW.Model
{
    /// <summary>Integer ramp preview matches the firmware's 0..1000 smoothstep blend.</summary>
    public static class ResponseMath
    {
        public static int Ramp(int progress, uint smoothness)
        {
            progress = Math.Clamp(progress, 0, 1000);
            smoothness = Math.Min(smoothness, 100);
            long smooth = (long)progress * progress * (3000 - 2 * progress) / 1000000;
            return (int)((progress * (100 - smoothness) + smooth * smoothness) / 100);
        }
        public static double RiseSeconds(uint amps, uint ampsPerSecond) => (double)amps / Math.Max(ampsPerSecond, 1);
    }
}
