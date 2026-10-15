using System;
using System.Linq;

namespace BBSFW.Model
{
    public sealed class VirtualTelemetry
    {
        public double Volts { get; private set; }
        public double Amps { get; private set; }
        public double Watts => Volts * Amps;
        public double SpeedKph { get; private set; }
        public byte AssistLevel { get; private set; }
        public byte Status { get; private set; }
        public byte Throttle { get; private set; }

        public static VirtualTelemetry Decode(byte[] data)
        {
            if (data.Length != 15 || data[0] != 1 || data[1] != 4 || data[2] != 1 ||
                (byte)data.Take(14).Sum(x => x) != data[14]) return null;
            int voltage = data[3] * 256 + data[4];
            int current = data[5] * 256 + data[6];
            int rpm = data[7] * 256 + data[8];
            int wheel = data[9] * 256 + data[10];
            return new VirtualTelemetry {
                Volts = voltage / 10.0, Amps = current / 10.0,
                SpeedKph = rpm / 10.0 * (wheel / 10.0) * 0.0254 * Math.PI * 60 / 1000,
                AssistLevel = data[11], Status = data[12], Throttle = data[13]
            };
        }
    }
}
