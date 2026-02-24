export interface PinState {
  pin: number;
  output: string;
  input: string;
}

export interface PortState {
  pins: PinState[];
}

export type GpioState = Record<string, PortState>;

export interface PwmChannel {
  channel: number;
  duty_cycle: number;
  frequency: number;
  enabled: boolean;
  polarity: number;
  period_us: number;
}

export interface AdcChannel {
  channel: number;
  raw_value: number;
  voltage: number;
  enabled: boolean;
  sample_rate: number;
}

export interface VoltaState {
  gpio: GpioState;
  pwm: PwmChannel[];
  adc: AdcChannel[];
  uart: string;
  connected: boolean;
}
