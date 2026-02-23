// VoltaUARTBridge — Renode peripheral that captures UART output to SHM ring buffer.
// Implements IConnectable<IUART> so it can be wired via: connector Connect usart2 voltaUart2Bridge
// Loaded via: include @/opt/volta/peripherals/VoltaUARTBridge.cs

using System;
using System.IO;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.UART;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class VoltaUARTBridge : IExternal, IConnectable<IUART>
    {
        public VoltaUARTBridge(int channelIndex = 0)
        {
            this.channelIndex = channelIndex;
            EnsureShmInitialized();
        }

        public void AttachTo(IUART uart)
        {
            uart.CharReceived += OnChar;
            this.Log(LogLevel.Info, "UART bridge attached to channel {0}", channelIndex);
        }

        public void DetachFrom(IUART uart)
        {
            uart.CharReceived -= OnChar;
        }

        public void Reset()
        {
            // Reset write head for this channel
            lock (shmLock)
            {
                int baseOffset = UART_OFFSET + channelIndex * UART_CHANNEL_SIZE;
                shmStream.Seek(baseOffset, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes((uint)0), 0, 4);
                shmStream.Flush();
            }
        }

        private void OnChar(byte c)
        {
            lock (shmLock)
            {
                int baseOffset = UART_OFFSET + channelIndex * UART_CHANNEL_SIZE;

                // Read write_head (u32)
                byte[] headBuf = new byte[4];
                shmStream.Seek(baseOffset, SeekOrigin.Begin);
                shmStream.Read(headBuf, 0, 4);
                uint writeHead = BitConverter.ToUInt32(headBuf, 0);

                // Write char into ring buffer at writeHead % UART_BUF_SIZE
                int dataOffset = baseOffset + UART_HEADER_SIZE + (int)(writeHead % UART_BUF_SIZE);
                shmStream.Seek(dataOffset, SeekOrigin.Begin);
                shmStream.WriteByte(c);

                // Advance write_head
                writeHead++;
                shmStream.Seek(baseOffset, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes(writeHead), 0, 4);

                // Bump global sequence counter
                byte[] seqBuf = new byte[8];
                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Read(seqBuf, 0, 8);
                long seq = BitConverter.ToInt64(seqBuf, 0) + 1;
                shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes(seq), 0, 8);

                shmStream.Flush();
            }
        }

        // ---- Static SHM state (shared with VoltaGPIOBridge) ----

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

                // Write header (idempotent if GPIO bridge already did it)
                shmStream.Seek(MAGIC_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes(MAGIC), 0, 4);
                shmStream.Seek(VERSION_OFFSET, SeekOrigin.Begin);
                shmStream.Write(BitConverter.GetBytes((uint)1), 0, 4);
                shmStream.Flush();

                shmReady = true;
            }
        }

        // ---- Layout constants (must match voltad) ----
        private const string SHM_PATH = "/dev/shm/volta_peripheral_state";
        private const int SHM_SIZE = 65536;

        private const int MAGIC_OFFSET = 0;
        private const int VERSION_OFFSET = 4;
        private const int SEQ_OFFSET = 8;

        private const uint MAGIC = 0x564F4C54; // "VOLT"

        // UART section layout
        private const int UART_OFFSET = 0x0140;
        private const int UART_CHANNEL_SIZE = 1024;
        private const int UART_HEADER_SIZE = 8;  // write_head(4) + reserved(4)
        private const int UART_BUF_SIZE = 1016;  // 1024 - 8

        // ---- Instance state ----
        private readonly int channelIndex; // 0-7 (maps to USART1..8)
    }
}
