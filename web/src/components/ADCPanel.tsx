import React from "react";
import type { AdcChannel } from "../types";

interface ADCPanelProps {
  channels: AdcChannel[];
}

const styles: Record<string, React.CSSProperties> = {
  container: {
    padding: "12px",
  },
  heading: {
    fontSize: "12px",
    fontWeight: 700,
    textTransform: "uppercase" as const,
    letterSpacing: "1px",
    color: "#e94560",
    marginBottom: "12px",
  },
  channelRow: {
    marginBottom: "12px",
  },
  channelLabel: {
    fontSize: "11px",
    color: "#aaa",
    marginBottom: "4px",
  },
  barContainer: {
    height: "12px",
    background: "#1a1a2e",
    borderRadius: "6px",
    overflow: "hidden",
    marginBottom: "4px",
  },
  barFill: {
    height: "100%",
    background: "linear-gradient(90deg, #00d2ff, #3a7bd5)",
    borderRadius: "6px",
    transition: "width 0.15s ease",
  },
  valueRow: {
    display: "flex",
    justifyContent: "space-between",
    fontSize: "12px",
    fontFamily: "monospace",
  },
  rawValue: {
    color: "#888",
  },
  voltageValue: {
    color: "#fff",
    fontWeight: 600,
  },
  injectRow: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
    marginTop: "8px",
  },
  slider: {
    flex: 1,
    accentColor: "#3a7bd5",
  },
  injectBtn: {
    padding: "4px 10px",
    fontSize: "11px",
    background: "#3a7bd5",
    color: "#fff",
    border: "none",
    borderRadius: "4px",
    cursor: "pointer",
  },
  voltLabel: {
    fontSize: "11px",
    color: "#aaa",
    width: "48px",
    textAlign: "right" as const,
    fontFamily: "monospace",
  },
  empty: {
    color: "#666",
    fontSize: "12px",
    padding: "8px",
  },
};

const VREF = 3.3;
const MAX_RAW = 4095;

export function ADCPanel({ channels }: ADCPanelProps) {
  const [injectVoltage, setInjectVoltage] = React.useState(0);
  const [injectChannel, setInjectChannel] = React.useState(0);

  const handleInject = async () => {
    try {
      await fetch(`/api/v1/adc/${injectChannel}/inject`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ voltage: injectVoltage }),
      });
    } catch {
      // Will be reflected on next state update
    }
  };

  return (
    <div style={styles.container}>
      <div style={styles.heading}>ADC Input</div>
      {channels.map((ch) => (
        <div key={ch.channel} style={styles.channelRow}>
          <div style={styles.channelLabel}>
            CH{ch.channel} {ch.enabled ? "" : "(OFF)"}
          </div>
          <div style={styles.barContainer}>
            <div
              style={{
                ...styles.barFill,
                width: `${(ch.raw_value / MAX_RAW) * 100}%`,
              }}
            />
          </div>
          <div style={styles.valueRow}>
            <span style={styles.rawValue}>raw: {ch.raw_value}</span>
            <span style={styles.voltageValue}>{ch.voltage.toFixed(3)} V</span>
          </div>
        </div>
      ))}
      {channels.length === 0 && (
        <div style={styles.empty}>No active ADC channels</div>
      )}

      <div style={{ ...styles.heading, marginTop: "16px" }}>Inject</div>
      <div style={styles.injectRow}>
        <span style={{ fontSize: "11px", color: "#aaa" }}>CH</span>
        <select
          value={injectChannel}
          onChange={(e) => setInjectChannel(Number(e.target.value))}
          style={{
            background: "#1a1a2e",
            color: "#fff",
            border: "1px solid #333",
            borderRadius: "4px",
            padding: "2px 4px",
            fontSize: "11px",
          }}
        >
          {Array.from({ length: 16 }, (_, i) => (
            <option key={i} value={i}>
              {i}
            </option>
          ))}
        </select>
      </div>
      <div style={styles.injectRow}>
        <input
          type="range"
          min={0}
          max={VREF}
          step={0.01}
          value={injectVoltage}
          onChange={(e) => setInjectVoltage(Number(e.target.value))}
          style={styles.slider}
        />
        <span style={styles.voltLabel}>{injectVoltage.toFixed(2)}V</span>
        <button onClick={handleInject} style={styles.injectBtn}>
          Set
        </button>
      </div>
    </div>
  );
}
