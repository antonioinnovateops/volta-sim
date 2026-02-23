import React from "react";

const styles: Record<string, React.CSSProperties> = {
  container: {
    display: "flex",
    flexDirection: "column",
    height: "100%",
    padding: "8px 12px",
  },
  header: {
    display: "flex",
    alignItems: "center",
    justifyContent: "space-between",
    marginBottom: "8px",
  },
  heading: {
    fontSize: "12px",
    fontWeight: 700,
    textTransform: "uppercase" as const,
    letterSpacing: "1px",
    color: "#e94560",
  },
  channelSelect: {
    background: "#1a1a2e",
    color: "#ccc",
    border: "1px solid #333",
    borderRadius: "4px",
    padding: "2px 6px",
    fontSize: "11px",
  },
  output: {
    flex: 1,
    background: "#0a0a0a",
    border: "1px solid #222",
    borderRadius: "4px",
    padding: "8px",
    fontFamily: "monospace",
    fontSize: "12px",
    lineHeight: "1.5",
    overflowY: "auto" as const,
    color: "#0f0",
    whiteSpace: "pre-wrap" as const,
    wordBreak: "break-all" as const,
  },
  empty: {
    color: "#333",
    fontStyle: "italic" as const,
  },
};

export function UARTConsole() {
  const [channel, setChannel] = React.useState(1); // USART2 = index 1
  const [data, setData] = React.useState("");
  const outputRef = React.useRef<HTMLDivElement>(null);

  React.useEffect(() => {
    const fetchUart = async () => {
      try {
        const res = await fetch(`/api/v1/uart/${channel}/buffer`);
        const json = await res.json();
        if (json.data) {
          setData(json.data);
        }
      } catch {
        // API unavailable
      }
    };

    fetchUart();
    const interval = setInterval(fetchUart, 500);
    return () => clearInterval(interval);
  }, [channel]);

  React.useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [data]);

  return (
    <div style={styles.container}>
      <div style={styles.header}>
        <span style={styles.heading}>UART Console</span>
        <select
          style={styles.channelSelect}
          value={channel}
          onChange={(e) => setChannel(Number(e.target.value))}
        >
          {[0, 1, 2, 3, 4, 5, 6, 7].map((ch) => (
            <option key={ch} value={ch}>
              UART{ch} {ch === 1 ? "(USART2)" : ""}
            </option>
          ))}
        </select>
      </div>
      <div ref={outputRef} style={styles.output}>
        {data || <span style={styles.empty}>No UART data received</span>}
      </div>
    </div>
  );
}
