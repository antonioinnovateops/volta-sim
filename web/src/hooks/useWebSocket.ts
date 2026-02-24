import { useCallback, useEffect, useRef, useState } from "react";
import type { VoltaState, GpioState, PwmChannel, AdcChannel } from "../types";

const INITIAL_STATE: VoltaState = {
  gpio: {},
  pwm: [],
  adc: [],
  uart: "",
  connected: false,
};

export function useVoltaState(): VoltaState {
  const [state, setState] = useState<VoltaState>(INITIAL_STATE);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout>>();
  const pollTimer = useRef<ReturnType<typeof setInterval>>();

  const fetchSnapshot = useCallback(async () => {
    try {
      const [gpioRes, pwmRes, adcRes, uartRes] = await Promise.all([
        fetch("/api/v1/gpio").then((r) => r.json()),
        fetch("/api/v1/pwm").then((r) => r.json()),
        fetch("/api/v1/adc").then((r) => r.json()),
        fetch("/api/v1/uart/1/buffer").then((r) => r.json()),
      ]);
      setState((prev) => ({
        ...prev,
        gpio: gpioRes.ports || prev.gpio,
        pwm: pwmRes.channels || prev.pwm,
        adc: adcRes.channels || prev.adc,
        uart: uartRes.data || prev.uart,
      }));
    } catch {
      // API not available yet
    }
  }, []);

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return;

    const proto = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${proto}//${window.location.host}/api/v1/simulation/ws/events`;
    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      setState((prev) => ({ ...prev, connected: true }));
      // Stop fallback polling when WS connects
      if (pollTimer.current) {
        clearInterval(pollTimer.current);
        pollTimer.current = undefined;
      }
    };

    ws.onmessage = (e) => {
      try {
        const event = JSON.parse(e.data);
        const { topic, data } = event;

        setState((prev) => {
          switch (topic) {
            case "volta.gpio.change": {
              const port = data.port as string;
              const pin = data.pin as number;
              const isHigh = data.state as boolean;
              const prevPort = prev.gpio[port] || { pins: [] };
              const existingPins = prevPort.pins.filter(
                (p) => p.pin !== pin
              );
              const updatedPins = [
                ...existingPins,
                {
                  pin,
                  output: isHigh ? "HIGH" : "LOW",
                  input: "LOW",
                },
              ].sort((a, b) => a.pin - b.pin);
              return {
                ...prev,
                gpio: {
                  ...prev.gpio,
                  [port]: { pins: updatedPins },
                } as GpioState,
              };
            }
            case "volta.pwm.update": {
              const ch = data as PwmChannel;
              const others = prev.pwm.filter(
                (p) => p.channel !== ch.channel
              );
              return {
                ...prev,
                pwm: [...others, ch].sort(
                  (a, b) => a.channel - b.channel
                ),
              };
            }
            case "volta.adc.change": {
              const ch = data as AdcChannel;
              const others = prev.adc.filter(
                (a) => a.channel !== ch.channel
              );
              return {
                ...prev,
                adc: [...others, ch].sort(
                  (a, b) => a.channel - b.channel
                ),
              };
            }
            case "volta.uart.data": {
              const bytes = data.data as number[];
              const text = String.fromCharCode(...bytes);
              return {
                ...prev,
                uart: prev.uart + text,
              };
            }
            default:
              return prev;
          }
        });
      } catch {
        // ignore parse errors
      }
    };

    ws.onclose = () => {
      setState((prev) => ({ ...prev, connected: false }));
      wsRef.current = null;
      // Start fallback polling
      if (!pollTimer.current) {
        pollTimer.current = setInterval(fetchSnapshot, 500);
      }
      // Reconnect with backoff
      reconnectTimer.current = setTimeout(connect, 3000);
    };

    ws.onerror = () => {
      ws.close();
    };
  }, [fetchSnapshot]);

  useEffect(() => {
    // Initial snapshot fetch
    fetchSnapshot();
    // Start WebSocket
    connect();
    // Also poll at 1Hz as baseline (WS events are deltas, not full snapshots)
    const baseline = setInterval(fetchSnapshot, 1000);

    return () => {
      clearInterval(baseline);
      if (pollTimer.current) clearInterval(pollTimer.current);
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      wsRef.current?.close();
    };
  }, [connect, fetchSnapshot]);

  return state;
}
