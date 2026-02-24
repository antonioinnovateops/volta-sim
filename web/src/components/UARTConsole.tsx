import React from "react";

interface UARTConsoleProps {
  data: string;
}

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

export function UARTConsole({ data }: UARTConsoleProps) {
  const outputRef = React.useRef<HTMLDivElement>(null);

  React.useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [data]);

  return (
    <div style={styles.container}>
      <div style={styles.header}>
        <span style={styles.heading}>UART Console (USART2)</span>
      </div>
      <div ref={outputRef} style={styles.output}>
        {data || <span style={styles.empty}>No UART data received</span>}
      </div>
    </div>
  );
}
