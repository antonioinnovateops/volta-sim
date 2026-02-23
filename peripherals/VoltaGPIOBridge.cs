// VoltaGPIOBridge — Renode peripheral that bridges GPIO pin state to /dev/shm.
// One instance per monitored pin (mirrors the LED peripheral pattern).
// Loaded via: include @/opt/volta/peripherals/VoltaGPIOBridge.cs

using System;
using System.IO;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class VoltaGPIOBridge : IGPIOReceiver, IExternal
    {
        public VoltaGPIOBridge(int portIndex = 0, int pinIndex = 0)
        {
            this.portIndex = portIndex;
            this.pinIndex = pinIndex;
            EnsureShmInitialized();
        }

        public void OnGPIO(int number, bool value)
        {
            lock (shmLock)
            {
                int offset = GPIO_OFFSET + portIndex * PORT_SIZE;

                // Read current output_state u16
                shmStream.Seek(offset, SeekOrigin.Begin);
                byte[] buf = new byte[2];
                shmStream.Read(buf, 0, 2);
                ushort current = (ushort)(buf[0] | (buf[1] << 8));

                if (value)
                    current |= (ushort)(1 << pinIndex);
                else
                    current &= (ushort)~(1 << pinIndex);

                buf[0] = (byte)(current & 0xFF);
                buf[1] = (byte)((current >> 8) & 0xFF);
                shmStream.Seek(offset, SeekOrigin.Begin);
                shmStream.Write(buf, 0, 2);

                // Increment sequence counter
                byte[] seqBuf = new byte[8];
                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Read(seqBuf, 0, 8);
                long seq = BitConverter.ToInt64(seqBuf, 0) + 1;
                byte[] seqOut = BitConverter.GetBytes(seq);
                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Write(seqOut, 0, 8);

                shmStream.Flush();
            }

            this.Log(LogLevel.Debug, "GPIO change: port={0} pin={1} state={2}",
                (char)('A' + portIndex), pinIndex, value ? "HIGH" : "LOW");
        }

        public void Reset()
        {
            lock (shmLock)
            {
                int offset = GPIO_OFFSET + portIndex * PORT_SIZE;
                byte[] buf = new byte[2];
                shmStream.Seek(offset, SeekOrigin.Begin);
                shmStream.Read(buf, 0, 2);
                ushort current = (ushort)(buf[0] | (buf[1] << 8));
                current &= (ushort)~(1 << pinIndex);
                buf[0] = (byte)(current & 0xFF);
                buf[1] = (byte)((current >> 8) & 0xFF);
                shmStream.Seek(offset, SeekOrigin.Begin);
                shmStream.Write(buf, 0, 2);

                byte[] seqBuf = new byte[8];
                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Read(seqBuf, 0, 8);
                long seq = BitConverter.ToInt64(seqBuf, 0) + 1;
                byte[] seqOut = BitConverter.GetBytes(seq);
                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Write(seqOut, 0, 8);

                shmStream.Flush();
            }
        }

        // ---- Static SHM state (shared across all instances) ----

        private static FileStream shmStream;
        private static readonly object shmLock = new object();
        private static bool shmReady = false;

        private static void EnsureShmInitialized()
        {
            if (shmReady) return;
            lock (shmLock)
            {
                if (shmReady) return;

                shmStream = new FileStream(SHM_PATH, FileMode.OpenOrCreate,
                    FileAccess.ReadWrite, FileShare.ReadWrite);

                if (shmStream.Length < SHM_SIZE)
                    shmStream.SetLength(SHM_SIZE);

                // Write header
                shmStream.Seek(MAGIC_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes(MAGIC), 0, 4);

                shmStream.Seek(VERSION_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes((uint)1), 0, 4);

                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes((long)0), 0, 8);

                shmStream.Seek(FLAGS_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes((uint)1), 0, 4); // initialized

                shmStream.Flush();
                shmReady = true;
            }
        }

        // ---- Layout constants ----
        private const string SHM_PATH = "/dev/shm/volta_peripheral_state";
        private const int SHM_SIZE = 65536;

        private const int MAGIC_OFFSET = 0;
        private const int VERSION_OFFSET = 4;
        private const int SEQ_OFFSET = 8;
        private const int FLAGS_OFFSET = 16;

        private const uint MAGIC = 0x564F4C54; // "VOLT"
        private const int GPIO_OFFSET = 0x0040;
        private const int PORT_SIZE = 16; // bytes per GPIO port

        // ---- Instance state ----
        private readonly int portIndex; // 0=A, 1=B, 2=C, 3=D, ...
        private readonly int pinIndex;  // 0-15
    }
}
