import React from "react";

const styles: Record<string, React.CSSProperties> = {
  container: {
    width: "100%",
    height: "100%",
    position: "relative",
  },
  iframe: {
    width: "100%",
    height: "100%",
    border: "none",
  },
  fallback: {
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    height: "100%",
    color: "#666",
    fontSize: "14px",
    flexDirection: "column",
    gap: "8px",
  },
};

export function PixelStreamViewer() {
  // Pixel Streaming is served by volta-ue5 on port 8888
  // In production, nginx proxies this; in dev, connect directly
  const streamUrl = window.location.port === "3000"
    ? "http://localhost:8888"
    : `${window.location.protocol}//${window.location.hostname}:8888`;

  const [loaded, setLoaded] = React.useState(false);
  const [error, setError] = React.useState(false);

  return (
    <div style={styles.container}>
      {!loaded && !error && (
        <div style={styles.fallback}>
          <span>Connecting to Pixel Streaming...</span>
          <span style={{ fontSize: "12px", color: "#444" }}>{streamUrl}</span>
        </div>
      )}
      {error && (
        <div style={styles.fallback}>
          <span>Pixel Streaming unavailable</span>
          <span style={{ fontSize: "12px", color: "#444" }}>
            Ensure volta-ue5 is running on port 8888
          </span>
        </div>
      )}
      <iframe
        src={streamUrl}
        style={{ ...styles.iframe, display: loaded ? "block" : "none" }}
        onLoad={() => setLoaded(true)}
        onError={() => setError(true)}
        allow="autoplay; fullscreen"
        title="Pixel Streaming"
      />
    </div>
  );
}
