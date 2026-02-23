// VoltaPWMBridge — Renode peripheral that replaces a timer to capture PWM state to SHM.
// Implements a minimal STM32 advanced timer register model (CR1, CCER, PSC, ARR, CCRx, BDTR).
// When firmware writes timer registers, computes duty cycle and frequency, then writes
// to the PWM section of shared memory. No background threads — purely event-driven.
//
// Usage: Register at the timer base address (e.g., TIM1 at 0x40010000) after unregistering
// the built-in timer model. See platforms/pwm_overlay.resc.

using System;
using System.IO;
using System.Threading;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class VoltaPWMBridge : BasicDoubleWordPeripheral, IKnownSize
    {
        public VoltaPWMBridge(IMachine machine, int pwmChannelIndex = 0,
            uint timerClockHz = 16000000) : base(machine)
        {
            this.pwmChannelIndex = pwmChannelIndex;
            this.timerClockHz = timerClockHz;

            EnsureShmInitialized();
            DefineRegisters();
        }

        public long Size => 0x400;

        public override void Reset()
        {
            base.Reset();
            timerEnabled = false;
            ccerValue = 0;
            pscValue = 0;
            arrValue = 0;
            bdtrValue = 0;
            for (int i = 0; i < 4; i++) ccrValues[i] = 0;

            // Clear PWM channel in SHM
            WritePwmToShm(0.0f, 0, false, 0, 0);
        }

        private void DefineRegisters()
        {
            // CR1 — Control Register 1 (offset 0x00)
            Registers.CR1.Define(this)
                .WithFlag(0, name: "CEN", writeCallback: (_, val) =>
                {
                    timerEnabled = val;
                    UpdatePwmState();
                })
                .WithTag("CR1_OTHER", 1, 31);

            // CR2 — Control Register 2 (offset 0x04)
            Registers.CR2.Define(this)
                .WithValueField(0, 32, name: "CR2");

            // SMCR — Slave Mode Control Register (offset 0x08)
            Registers.SMCR.Define(this)
                .WithValueField(0, 32, name: "SMCR");

            // DIER — DMA/Interrupt Enable Register (offset 0x0C)
            Registers.DIER.Define(this)
                .WithValueField(0, 32, name: "DIER");

            // SR — Status Register (offset 0x10)
            Registers.SR.Define(this)
                .WithFlag(0, FieldMode.Read, name: "UIF", valueProviderCallback: _ => true)
                .WithTag("SR_OTHER", 1, 31);

            // EGR — Event Generation Register (offset 0x14)
            Registers.EGR.Define(this)
                .WithValueField(0, 32, name: "EGR");

            // CCMR1 — Capture/Compare Mode Register 1 (offset 0x18)
            Registers.CCMR1.Define(this)
                .WithValueField(0, 32, name: "CCMR1");

            // CCMR2 — Capture/Compare Mode Register 2 (offset 0x1C)
            Registers.CCMR2.Define(this)
                .WithValueField(0, 32, name: "CCMR2");

            // CCER — Capture/Compare Enable Register (offset 0x20)
            Registers.CCER.Define(this)
                .WithValueField(0, 32, name: "CCER", writeCallback: (_, val) =>
                {
                    ccerValue = (uint)val;
                    UpdatePwmState();
                });

            // CNT — Counter (offset 0x24)
            Registers.CNT.Define(this)
                .WithValueField(0, 32, name: "CNT");

            // PSC — Prescaler (offset 0x28)
            Registers.PSC.Define(this)
                .WithValueField(0, 16, name: "PSC", writeCallback: (_, val) =>
                {
                    pscValue = (uint)val;
                    UpdatePwmState();
                });

            // ARR — Auto-Reload Register (offset 0x2C)
            Registers.ARR.Define(this)
                .WithValueField(0, 32, name: "ARR", writeCallback: (_, val) =>
                {
                    arrValue = (uint)val;
                    UpdatePwmState();
                });

            // RCR — Repetition Counter Register (offset 0x30)
            Registers.RCR.Define(this)
                .WithValueField(0, 32, name: "RCR");

            // CCR1 — Capture/Compare Register 1 (offset 0x34)
            Registers.CCR1.Define(this)
                .WithValueField(0, 32, name: "CCR1", writeCallback: (_, val) =>
                {
                    ccrValues[0] = (uint)val;
                    UpdatePwmState();
                });

            // CCR2 (offset 0x38)
            Registers.CCR2.Define(this)
                .WithValueField(0, 32, name: "CCR2", writeCallback: (_, val) =>
                {
                    ccrValues[1] = (uint)val;
                });

            // CCR3 (offset 0x3C)
            Registers.CCR3.Define(this)
                .WithValueField(0, 32, name: "CCR3", writeCallback: (_, val) =>
                {
                    ccrValues[2] = (uint)val;
                });

            // CCR4 (offset 0x40)
            Registers.CCR4.Define(this)
                .WithValueField(0, 32, name: "CCR4", writeCallback: (_, val) =>
                {
                    ccrValues[3] = (uint)val;
                });

            // BDTR — Break and Dead-Time Register (offset 0x44)
            Registers.BDTR.Define(this)
                .WithValueField(0, 32, name: "BDTR", writeCallback: (_, val) =>
                {
                    bdtrValue = (uint)val;
                    UpdatePwmState();
                });

            // DCR — DMA Control Register (offset 0x48)
            Registers.DCR.Define(this)
                .WithValueField(0, 32, name: "DCR");

            // DMAR — DMA address for full transfer (offset 0x4C)
            Registers.DMAR.Define(this)
                .WithValueField(0, 32, name: "DMAR");
        }

        private void UpdatePwmState()
        {
            // We track CH1 for pwmChannelIndex 0
            int ch = 0;

            // Check MOE (Main Output Enable) in BDTR bit 15 — required for TIM1 advanced timer
            bool moeEnabled = (bdtrValue & (1u << 15)) != 0;

            // Check CEN (Counter Enable) in CR1
            if (!timerEnabled)
            {
                WritePwmToShm(0.0f, 0, false, 0, 0);
                return;
            }

            // Check CC1E in CCER (bit 0 for CH1, bit 4 for CH2, etc.)
            bool channelEnabled = moeEnabled && ((ccerValue & (1u << (ch * 4))) != 0);

            uint frequency = 0;
            float dutyCycle = 0.0f;
            uint periodUs = 0;

            if (arrValue > 0 && channelEnabled)
            {
                uint divisor = (pscValue + 1) * (arrValue + 1);
                frequency = timerClockHz / divisor;
                dutyCycle = (float)ccrValues[ch] / (float)(arrValue + 1);
                if (dutyCycle > 1.0f) dutyCycle = 1.0f;
                periodUs = (uint)((1000000UL * divisor) / timerClockHz);
            }

            // Check polarity: CC1P = CCER bit 1
            byte polarity = (byte)(((ccerValue >> (ch * 4 + 1)) & 1) != 0 ? 1 : 0);

            WritePwmToShm(dutyCycle, frequency, channelEnabled, polarity, periodUs);
        }

        private void WritePwmToShm(float dutyCycle, uint frequency, bool enabled, byte polarity, uint periodUs)
        {
            lock (shmLock)
            {
                int offset = PWM_OFFSET + pwmChannelIndex * PWM_CHANNEL_SIZE;

                // duty_cycle f32 at +0x00
                byte[] dutyBytes = BitConverter.GetBytes(dutyCycle);
                shmStream.Seek(offset + 0, SeekOrigin.Begin);
                shmStream.Write(dutyBytes, 0, 4);

                // frequency u32 at +0x04
                byte[] freqBytes = BitConverter.GetBytes(frequency);
                shmStream.Seek(offset + 4, SeekOrigin.Begin);
                shmStream.Write(freqBytes, 0, 4);

                // enabled u8 at +0x08
                shmStream.Seek(offset + 8, SeekOrigin.Begin);
                shmStream.WriteByte(enabled ? (byte)1 : (byte)0);

                // polarity u8 at +0x09
                shmStream.Seek(offset + 9, SeekOrigin.Begin);
                shmStream.WriteByte(polarity);

                // period_us u32 at +0x0C
                byte[] periodBytes = BitConverter.GetBytes(periodUs);
                shmStream.Seek(offset + 12, SeekOrigin.Begin);
                shmStream.Write(periodBytes, 0, 4);

                IncrementSequence();
                shmStream.Flush();
            }

            this.Log(LogLevel.Debug, "PWM update: ch={0} duty={1:F3} freq={2}Hz enabled={3}",
                pwmChannelIndex, dutyCycle, frequency, enabled);
        }

        private static void IncrementSequence()
        {
            byte[] seqBuf = new byte[8];
            shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
            shmStream.Read(seqBuf, 0, 8);
            long seq = BitConverter.ToInt64(seqBuf, 0) + 1;
            byte[] seqOut = BitConverter.GetBytes(seq);
            shmStream.Seek(SEQ_OFFSET, SeekOrigin.Begin);
            shmStream.Write(seqOut, 0, 8);
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
        private const int SEQ_OFFSET = 8;
        private const int PWM_OFFSET = 0x2140;
        private const int PWM_CHANNEL_SIZE = 16;

        // ---- Instance state ----
        private readonly int pwmChannelIndex;
        private readonly uint timerClockHz;
        private bool timerEnabled;
        private uint ccerValue;
        private uint pscValue;
        private uint arrValue;
        private uint bdtrValue;
        private uint[] ccrValues = new uint[4];

        // ---- Register map (STM32 TIMx offsets) ----
        private enum Registers
        {
            CR1 = 0x00,
            CR2 = 0x04,
            SMCR = 0x08,
            DIER = 0x0C,
            SR = 0x10,
            EGR = 0x14,
            CCMR1 = 0x18,
            CCMR2 = 0x1C,
            CCER = 0x20,
            CNT = 0x24,
            PSC = 0x28,
            ARR = 0x2C,
            RCR = 0x30,
            CCR1 = 0x34,
            CCR2 = 0x38,
            CCR3 = 0x3C,
            CCR4 = 0x40,
            BDTR = 0x44,
            DCR = 0x48,
            DMAR = 0x4C,
        }
    }
}
