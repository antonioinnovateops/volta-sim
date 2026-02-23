import React from "react";

interface PwmChannel {
  channel: number;
  duty_cycle: number;
  frequency: number;
  enabled: boolean;
  polarity: number;
  period_us: number;
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
  gaugeWrapper: {
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    marginBottom: "12px",
  },
  label: {
    fontSize: "11px",
    color: "#aaa",
    marginBottom: "4px",
  },
  freqText: {
    fontSize: "11px",
    color: "#888",
    marginTop: "4px",
  },
  dutyText: {
    fontSize: "18px",
    fontWeight: 700,
    fontFamily: "monospace",
    color: "#fff",
  },
  error: {
    color: "#666",
    fontSize: "12px",
    padding: "8px",
  },
};

function SemiCircleGauge({ value, size = 120 }: { value: number; size?: number }) {
  const r = size / 2 - 8;
  const cx = size / 2;
  const cy = size / 2 + 4;
  const startAngle = Math.PI;
  const endAngle = 0;
  const clampedValue = Math.max(0, Math.min(1, value));
  const sweepAngle = startAngle - (startAngle - endAngle) * clampedValue;

  const bgX1 = cx + r * Math.cos(startAngle);
  const bgY1 = cy - r * Math.sin(startAngle);
  const bgX2 = cx + r * Math.cos(endAngle);
  const bgY2 = cy - r * Math.sin(endAngle);

  const valX = cx + r * Math.cos(sweepAngle);
  const valY = cy - r * Math.sin(sweepAngle);

  const largeArc = clampedValue > 0.5 ? 1 : 0;

  return (
    <svg width={size} height={size / 2 + 12} viewBox={`0 0 ${size} ${size / 2 + 12}`}>
      {/* Background arc */}
      <path
        d={`M ${bgX1} ${bgY1} A ${r} ${r} 0 1 1 ${bgX2} ${bgY2}`}
        fill="none"
        stroke="#1a1a2e"
        strokeWidth="6"
        strokeLinecap="round"
      />
      {/* Value arc */}
      {clampedValue > 0.001 && (
        <path
          d={`M ${bgX1} ${bgY1} A ${r} ${r} 0 ${largeArc} 1 ${valX} ${valY}`}
          fill="none"
          stroke="#e94560"
          strokeWidth="6"
          strokeLinecap="round"
        />
      )}
    </svg>
  );
}

export function PWMGauge() {
  const [channels, setChannels] = React.useState<PwmChannel[]>([]);
  const [error, setError] = React.useState<string | null>(null);

  React.useEffect(() => {
    const fetchPwm = async () => {
      try {
        const res = await fetch("/api/v1/pwm");
        const data = await res.json();
        if (data.error) {
          setError(data.error);
        } else {
          setChannels(data.channels || []);
          setError(null);
        }
      } catch {
        setError("API unavailable");
      }
    };

    fetchPwm();
    const interval = setInterval(fetchPwm, 200); // Poll at 5Hz
    return () => clearInterval(interval);
  }, []);

  if (error) {
    return (
      <div style={styles.container}>
        <div style={styles.heading}>PWM Output</div>
        <div style={styles.error}>{error}</div>
      </div>
    );
  }

  return (
    <div style={styles.container}>
      <div style={styles.heading}>PWM Output</div>
      {channels.length === 0 && (
        <div style={styles.error}>No active PWM channels</div>
      )}
      {channels.map((ch) => (
        <div key={ch.channel} style={styles.gaugeWrapper}>
          <div style={styles.label}>
            CH{ch.channel} {ch.enabled ? "" : "(OFF)"}
          </div>
          <SemiCircleGauge value={ch.duty_cycle} />
          <div style={styles.dutyText}>
            {(ch.duty_cycle * 100).toFixed(1)}%
          </div>
          <div style={styles.freqText}>{ch.frequency} Hz</div>
        </div>
      ))}
    </div>
  );
}
