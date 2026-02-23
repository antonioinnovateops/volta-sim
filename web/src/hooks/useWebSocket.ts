import { useEffect, useRef, useState } from "react";

export function useWebSocket(url: string) {
  const [events, setEvents] = useState<unknown[]>([]);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    const wsUrl = `${window.location.protocol === "https:" ? "wss:" : "ws:"}//${window.location.host}${url}`;
    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        setEvents((prev) => [...prev.slice(-99), data]);
      } catch {
        // ignore parse errors
      }
    };

    ws.onclose = () => {
      // Reconnect after 3s
      setTimeout(() => {
        wsRef.current = new WebSocket(wsUrl);
      }, 3000);
    };

    return () => {
      ws.close();
    };
  }, [url]);

  return events;
}
