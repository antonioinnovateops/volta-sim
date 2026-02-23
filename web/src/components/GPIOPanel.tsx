import React from "react";

interface GPIOPanelProps {
  events: unknown[];
}

interface PinState {
  pin: number;
  output: string;
  input: string;
}

interface PortState {
  [port: string]: { pins: PinState[] };
}

const LED_COLORS: Record<number, string> = {
  12: "#00ff00", // Green
  13: "#ff8800", // Orange
  14: "#ff0000", // Red
  15: "#0088ff", // Blue
};

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
  portSection: {
    marginBottom: "16px",
  },
  portLabel: {
    fontSize: "11px",
    fontWeight: 600,
    color: "#aaa",
    marginBottom: "6px",
  },
  pinRow: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
    padding: "4px 0",
    fontSize: "12px",
    fontFamily: "monospace",
  },
  led: {
    width: "10px",
    height: "10px",
    borderRadius: "50%",
    border: "1px solid #333",
    flexShrink: 0,
  },
  pinName: {
    width: "40px",
    color: "#888",
  },
  pinState: {
    fontWeight: 600,
  },
  error: {
    color: "#666",
    fontSize: "12px",
    padding: "8px",
  },
};

export function GPIOPanel({ events: _events }: GPIOPanelProps) {
  const [ports, setPorts] = React.useState<PortState>({});
  const [error, setError] = React.useState<string | null>(null);

  React.useEffect(() => {
    const fetchGpio = async () => {
      try {
        const res = await fetch("/api/v1/gpio");
        const data = await res.json();
        if (data.error) {
          setError(data.error);
        } else {
          setPorts(data.ports || {});
          setError(null);
        }
      } catch {
        setError("API unavailable");
      }
    };

    fetchGpio();
    const interval = setInterval(fetchGpio, 200); // Poll at 5Hz
    return () => clearInterval(interval);
  }, []);

  if (error) {
    return (
      <div style={styles.container}>
        <div style={styles.heading}>GPIO State</div>
        <div style={styles.error}>{error}</div>
      </div>
    );
  }

  return (
    <div style={styles.container}>
      <div style={styles.heading}>GPIO State</div>
      {Object.entries(ports).map(([port, { pins }]) => (
        <div key={port} style={styles.portSection}>
          <div style={styles.portLabel}>Port {port}</div>
          {pins.map((pin) => {
            const isHigh = pin.output === "HIGH";
            const color = LED_COLORS[pin.pin] || "#888";
            return (
              <div key={pin.pin} style={styles.pinRow}>
                <div
                  style={{
                    ...styles.led,
                    background: isHigh ? color : "#1a1a1a",
                    boxShadow: isHigh ? `0 0 6px ${color}` : "none",
                  }}
                />
                <span style={styles.pinName}>P{port}{pin.pin}</span>
                <span
                  style={{
                    ...styles.pinState,
                    color: isHigh ? color : "#444",
                  }}
                >
                  {pin.output}
                </span>
              </div>
            );
          })}
        </div>
      ))}
      {Object.keys(ports).length === 0 && (
        <div style={styles.error}>No active pins</div>
      )}
    </div>
  );
}
