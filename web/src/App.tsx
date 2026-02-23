import React from "react";
import { PixelStreamViewer } from "./components/PixelStreamViewer";
import { GPIOPanel } from "./components/GPIOPanel";
import { PWMGauge } from "./components/PWMGauge";
import { ADCPanel } from "./components/ADCPanel";
import { UARTConsole } from "./components/UARTConsole";
import { SimulationControls } from "./components/SimulationControls";
import { useWebSocket } from "./hooks/useWebSocket";

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
  title: {
    fontSize: "16px",
    fontWeight: 700,
    color: "#e94560",
    letterSpacing: "2px",
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
  const events = useWebSocket("/api/v1/simulation/ws/events");

  return (
    <div style={styles.container}>
      <div style={styles.header}>
        <span style={styles.title}>VOLTA-SIM</span>
        <SimulationControls />
      </div>
      <div style={styles.stream}>
        <PixelStreamViewer />
      </div>
      <div style={styles.sidebar}>
        <GPIOPanel events={events} />
        <PWMGauge />
        <ADCPanel />
      </div>
      <div style={styles.console}>
        <UARTConsole />
      </div>
    </div>
  );
}
