// VoltaGPIOInputBridge — Renode peripheral that reads button input_state from SHM.
// Periodic polling reads input_state from /dev/shm and drives GPIO pins in Renode.
// This is the reverse path of VoltaGPIOBridge: UE5 writes -> SHM -> Renode reads.

using System;
using System.IO;
using System.Threading;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class VoltaGPIOInputBridge : IExternal, IGPIOSender, IDisposable
    {
        public VoltaGPIOInputBridge(int portIndex = 0, int pinIndex = 0, int pollIntervalMs = 1)
        {
            this.portIndex = portIndex;
            this.pinIndex = pinIndex;
            this.pollIntervalMs = pollIntervalMs;
            this.lastState = false;

            EnsureShmInitialized();
            StartPolling();
        }

        public GPIO Connection { get; } = new GPIO();

        public void Dispose()
        {
            StopPolling();
        }

        public void Reset()
        {
            lastState = false;
            Connection.Unset();
        }

        // ---- Polling logic ----

        private void StartPolling()
        {
            running = true;
            pollThread = new Thread(PollLoop)
            {
                IsBackground = true,
                Name = $"VoltaGPIOInput_P{(char)('A' + portIndex)}{pinIndex}"
            };
            pollThread.Start();
        }

        private void StopPolling()
        {
            running = false;
            if (pollThread != null && pollThread.IsAlive)
            {
                pollThread.Join(pollIntervalMs * 10);
            }
        }

        private void PollLoop()
        {
            while (running)
            {
                try
                {
                    bool currentState = ReadInputPin();

                    if (currentState != lastState)
                    {
                        lastState = currentState;

                        if (currentState)
                            Connection.Set();
                        else
                            Connection.Unset();

                        this.Log(LogLevel.Debug, "GPIO input change: port={0} pin={1} state={2}",
                            (char)('A' + portIndex), pinIndex, currentState ? "HIGH" : "LOW");
                    }
                }
                catch (Exception ex)
                {
                    this.Log(LogLevel.Warning, "GPIO input poll error: {0}", ex.Message);
                }

                Thread.Sleep(pollIntervalMs);
            }
        }

        private bool ReadInputPin()
        {
            lock (shmLock)
            {
                int offset = GPIO_OFFSET + portIndex * PORT_SIZE + INPUT_STATE_OFFSET;
                shmStream.Seek(offset, SeekOrigin.Begin);
                byte[] buf = new byte[2];
                shmStream.Read(buf, 0, 2);
                ushort inputState = (ushort)(buf[0] | (buf[1] << 8));
                return (inputState & (1 << pinIndex)) != 0;
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

                // Wait for SHM file to exist (Renode may start before VoltaGPIOBridge creates it)
                int retries = 0;
                while (!File.Exists(SHM_PATH) && retries < 30)
                {
                    Thread.Sleep(100);
                    retries++;
                }

                shmStream = new FileStream(SHM_PATH, FileMode.OpenOrCreate,
                    FileAccess.ReadWrite, FileShare.ReadWrite);

                if (shmStream.Length < SHM_SIZE)
                    shmStream.SetLength(SHM_SIZE);

                shmReady = true;
            }
        }

        // ---- Layout constants (must match VoltaGPIOBridge.cs) ----
        private const string SHM_PATH = "/dev/shm/volta_peripheral_state";
        private const int SHM_SIZE = 65536;
        private const int GPIO_OFFSET = 0x0040;
        private const int PORT_SIZE = 16;
        private const int INPUT_STATE_OFFSET = 2; // u16 input_state within port

        // ---- Instance state ----
        private readonly int portIndex;
        private readonly int pinIndex;
        private readonly int pollIntervalMs;
        private bool lastState;
        private Thread pollThread;
        private volatile bool running;
    }
}
