import React from "react";
import { BoardViewer } from "./components/BoardViewer";
import { GPIOPanel } from "./components/GPIOPanel";
import { PWMGauge } from "./components/PWMGauge";
import { ADCPanel } from "./components/ADCPanel";
import { UARTConsole } from "./components/UARTConsole";
import { SimulationControls } from "./components/SimulationControls";
import { useVoltaState } from "./hooks/useWebSocket";

const styles: Record<string, React.CSSProperties> = {
  container: {
    display: "grid",
    gridTemplateColumns: "1fr 360px",
    gridTemplateRows: "auto 1fr 280px",
    height: "100vh",
    gap: "1px",
    background: "#0f0f23",
  },
  header: {
    gridColumn: "1 / -1",
    padding: "8px 16px",
    background: "#16213e",
    display: "flex",
    alignItems: "center",
    justifyContent: "space-between",
    borderBottom: "1px solid #2a2a4a",
  },
  titleArea: {
    display: "flex",
    alignItems: "center",
    gap: "12px",
  },
  title: {
    fontSize: "16px",
    fontWeight: 700,
    color: "#e94560",
    letterSpacing: "2px",
  },
  connDot: {
    width: "8px",
    height: "8px",
    borderRadius: "50%",
    display: "inline-block",
  },
  connLabel: {
    fontSize: "10px",
    color: "#666",
    textTransform: "uppercase" as const,
    letterSpacing: "1px",
  },
  stream: {
    background: "#000",
    overflow: "hidden",
  },
  sidebar: {
    background: "#16213e",
    display: "flex",
    flexDirection: "column",
    gap: "1px",
    overflow: "auto",
  },
  console: {
    gridColumn: "1 / -1",
    background: "#0a0a1a",
    borderTop: "1px solid #2a2a4a",
  },
};

export function App() {
  const state = useVoltaState();

  return (
    <div style={styles.container}>
      <div style={styles.header}>
        <div style={styles.titleArea}>
          <span style={styles.title}>VOLTA-SIM</span>
          <span
            style={{
              ...styles.connDot,
              background: state.connected ? "#00ff00" : "#ff4444",
              boxShadow: state.connected
                ? "0 0 6px #00ff00"
                : "0 0 6px #ff4444",
            }}
          />
          <span style={styles.connLabel}>
            {state.connected ? "LIVE" : "POLL"}
          </span>
        </div>
        <SimulationControls />
      </div>
      <div style={styles.stream}>
        <BoardViewer
          gpio={state.gpio}
          pwm={state.pwm}
          adc={state.adc}
        />
      </div>
      <div style={styles.sidebar}>
        <GPIOPanel ports={state.gpio} />
        <PWMGauge channels={state.pwm} />
        <ADCPanel channels={state.adc} />
      </div>
      <div style={styles.console}>
        <UARTConsole data={state.uart} />
      </div>
    </div>
  );
}
