// VoltaADCBridge — Renode peripheral that emulates ADC1 at 0x40012000.
// Implements a minimal register model: CR2, SR, SQR3, DR.
// When firmware reads DR, returns raw_value from SHM ADC section.
// This replaces the built-in ADC for simplicity.

using System;
using System.IO;
using System.Threading;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Analog
{
    public class VoltaADCBridge : BasicDoubleWordPeripheral, IKnownSize
    {
        public VoltaADCBridge(IMachine machine) : base(machine)
        {
            EnsureShmInitialized();
            DefineRegisters();
        }

        public long Size => 0x400;

        private void DefineRegisters()
        {
            // SR — Status Register (offset 0x00)
            Registers.SR.Define(this)
                .WithFlag(1, FieldMode.Read, name: "EOC", valueProviderCallback: _ => eocFlag)
                .WithFlag(4, FieldMode.Read, name: "STRT", valueProviderCallback: _ => startedFlag);

            // CR2 — Control Register 2 (offset 0x08)
            Registers.CR2.Define(this)
                .WithFlag(0, name: "ADON", writeCallback: (_, val) => { adcEnabled = val; })
                .WithFlag(30, name: "SWSTART", writeCallback: (_, val) =>
                {
                    if (val && adcEnabled)
                    {
                        // Trigger a conversion — read from SHM immediately
                        startedFlag = true;
                        eocFlag = true;
                        lastConvertedValue = ReadAdcFromShm(selectedChannel);
                    }
                });

            // SQR3 — Regular Sequence Register 3 (offset 0x34)
            // Bits [4:0] = SQ1: first channel in sequence
            Registers.SQR3.Define(this)
                .WithValueField(0, 5, name: "SQ1", writeCallback: (_, val) =>
                {
                    selectedChannel = (int)val;
                });

            // DR — Data Register (offset 0x4C)
            Registers.DR.Define(this)
                .WithValueField(0, 16, FieldMode.Read, name: "DATA", valueProviderCallback: _ =>
                {
                    eocFlag = false;
                    startedFlag = false;
                    return (uint)lastConvertedValue;
                });
        }

        private ushort ReadAdcFromShm(int channel)
        {
            if (channel < 0 || channel >= ADC_CHANNEL_COUNT) return 0;

            lock (shmLock)
            {
                int offset = ADC_OFFSET + channel * ADC_CHANNEL_SIZE + ADC_RAW_VALUE_OFFSET;
                shmStream.Seek(offset, SeekOrigin.Begin);
                byte[] buf = new byte[2];
                shmStream.Read(buf, 0, 2);
                return (ushort)(buf[0] | (buf[1] << 8));
            }
        }

        // ---- Instance state ----
        private bool adcEnabled;
        private bool eocFlag;
        private bool startedFlag;
        private int selectedChannel;
        private ushort lastConvertedValue;

        // ---- Static SHM state ----
        private static FileStream shmStream;
        private static readonly object shmLock = new object();
        private static bool shmReady = false;

        private static void EnsureShmInitialized()
        {
            if (shmReady) return;
            lock (shmLock)
            {
                if (shmReady) return;

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

        // ---- SHM layout constants ----
        private const string SHM_PATH = "/dev/shm/volta_peripheral_state";
        private const int SHM_SIZE = 65536;
        private const int ADC_OFFSET = 0x2240;
        private const int ADC_CHANNEL_SIZE = 16;
        private const int ADC_CHANNEL_COUNT = 16;
        private const int ADC_RAW_VALUE_OFFSET = 0;

        // ---- Register map ----
        private enum Registers
        {
            SR = 0x00,
            CR2 = 0x08,
            SQR3 = 0x34,
            DR = 0x4C,
        }
    }
}
