import React from "react";

const styles: Record<string, React.CSSProperties> = {
  container: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
  },
  button: {
    padding: "4px 12px",
    border: "1px solid #333",
    borderRadius: "4px",
    background: "#1a1a2e",
    color: "#ccc",
    cursor: "pointer",
    fontSize: "12px",
    fontFamily: "inherit",
  },
  status: {
    fontSize: "12px",
    padding: "4px 8px",
    borderRadius: "4px",
    fontWeight: 600,
    textTransform: "uppercase" as const,
    letterSpacing: "1px",
  },
  flashInput: {
    background: "#1a1a2e",
    color: "#ccc",
    border: "1px solid #333",
    borderRadius: "4px",
    padding: "4px 8px",
    fontSize: "12px",
    width: "280px",
    fontFamily: "monospace",
  },
};

const STATUS_COLORS: Record<string, string> = {
  running: "#00ff00",
  stopped: "#ff4444",
  paused: "#ffaa00",
};

export function SimulationControls() {
  const [status, setStatus] = React.useState("unknown");
  const [elfPath, setElfPath] = React.useState(
    "/opt/volta/firmware/examples/blinky/blinky.elf"
  );
  const [busy, setBusy] = React.useState(false);

  const fetchStatus = React.useCallback(async () => {
    try {
      const res = await fetch("/api/v1/simulation/status");
      const data = await res.json();
      setStatus(data.state || "unknown");
    } catch {
      setStatus("offline");
    }
  }, []);

  React.useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 2000);
    return () => clearInterval(interval);
  }, [fetchStatus]);

  const doAction = async (action: string, body?: object) => {
    setBusy(true);
    try {
      await fetch(`/api/v1/${action}`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: body ? JSON.stringify(body) : undefined,
      });
      await fetchStatus();
    } catch {
      // ignore
    }
    setBusy(false);
  };

  return (
    <div style={styles.container}>
      <span
        style={{
          ...styles.status,
          color: STATUS_COLORS[status] || "#666",
        }}
      >
        {status}
      </span>

      <input
        style={styles.flashInput}
        value={elfPath}
        onChange={(e) => setElfPath(e.target.value)}
        placeholder="ELF path in container"
      />

      <button
        style={{ ...styles.button, borderColor: "#e94560", color: "#e94560" }}
        disabled={busy}
        onClick={() => {
          const form = new FormData();
          form.append("elf_path", elfPath);
          form.append("start", "true");
          fetch("/api/v1/firmware/flash", { method: "POST", body: form }).then(
            fetchStatus
          );
        }}
      >
        Flash + Start
      </button>

      <button
        style={{ ...styles.button, borderColor: "#00aa00", color: "#00aa00" }}
        disabled={busy || status === "running"}
        onClick={() => doAction("simulation/start")}
      >
        Start
      </button>

      <button
        style={{ ...styles.button, borderColor: "#ff4444", color: "#ff4444" }}
        disabled={busy || status === "stopped"}
        onClick={() => doAction("simulation/stop")}
      >
        Stop
      </button>
    </div>
  );
}
