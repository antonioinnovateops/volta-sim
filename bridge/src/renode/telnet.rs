use anyhow::{Context, Result};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::time::Duration;

// Telnet protocol constants
const IAC: u8 = 255;
const WILL: u8 = 251;
const WONT: u8 = 252;
const DO: u8 = 253;
const DONT: u8 = 254;

/// Synchronous telnet client for the Renode monitor.
pub struct RenodeTelnet {
    stream: TcpStream,
}

impl RenodeTelnet {
    pub fn connect(addr: &str) -> Result<Self> {
        let stream = TcpStream::connect(addr)
            .with_context(|| format!("Failed to connect to Renode at {addr}"))?;

        let mut client = Self { stream };

        // Handle telnet negotiation and wait for prompt
        client.initialize()?;

        Ok(client)
    }

    /// Connect with retries (Renode may not be ready immediately).
    pub fn connect_with_retry(addr: &str, max_attempts: u32) -> Result<Self> {
        for attempt in 1..=max_attempts {
            match Self::connect(addr) {
                Ok(client) => return Ok(client),
                Err(e) => {
                    if attempt == max_attempts {
                        return Err(e);
                    }
                    tracing::debug!(
                        "Renode telnet not ready (attempt {attempt}/{max_attempts}): {e}"
                    );
                    std::thread::sleep(Duration::from_secs(1));
                }
            }
        }
        unreachable!()
    }

    /// Send a command and return the response (blocking).
    pub fn send_command(&mut self, cmd: &str) -> Result<String> {
        // Drain any stale data
        self.drain_pending();

        // Send command
        self.stream.write_all(cmd.as_bytes())?;
        self.stream.write_all(b"\n")?;
        self.stream.flush()?;

        // Read response until prompt
        let raw = self.read_until_prompt(Duration::from_secs(10));

        // Extract just the response (skip echo and prompt)
        let lines: Vec<&str> = raw.lines().collect();
        let response: Vec<&str> = lines
            .iter()
            .copied()
            .filter(|l| {
                let t = l.trim();
                !t.is_empty() && !is_prompt(t) && !t.contains(cmd.trim())
            })
            .collect();

        Ok(response.join("\n").trim().to_string())
    }

    /// Initialize the connection: handle telnet negotiation and wait for prompt.
    fn initialize(&mut self) -> Result<()> {
        self.stream
            .set_read_timeout(Some(Duration::from_secs(2)))
            .ok();

        // Read and handle telnet negotiation
        let mut buf = [0u8; 4096];
        let mut text_data = Vec::new();
        let start = std::time::Instant::now();

        // Phase 1: Handle telnet negotiation (first few seconds)
        while start.elapsed() < Duration::from_secs(3) {
            match self.stream.read(&mut buf) {
                Ok(0) => break,
                Ok(n) => {
                    let data = &buf[..n];
                    let remaining = self.handle_telnet_negotiation(data);
                    text_data.extend_from_slice(&remaining);
                }
                Err(e)
                    if e.kind() == std::io::ErrorKind::TimedOut
                        || e.kind() == std::io::ErrorKind::WouldBlock =>
                {
                    break;
                }
                Err(e) => return Err(e.into()),
            }
        }

        // Phase 2: Send a newline and wait for prompt (Renode may be loading scripts)
        self.stream.write_all(b"\n")?;
        self.stream.flush()?;

        let output = self.read_until_prompt(Duration::from_secs(30));
        tracing::debug!(
            "Renode initial prompt received ({} chars)",
            output.len()
        );

        Ok(())
    }

    /// Handle telnet IAC negotiation sequences.
    /// Returns the non-IAC data (actual text content).
    fn handle_telnet_negotiation(&mut self, data: &[u8]) -> Vec<u8> {
        let mut text = Vec::new();
        let mut i = 0;

        while i < data.len() {
            if data[i] == IAC && i + 2 < data.len() {
                let cmd = data[i + 1];
                let opt = data[i + 2];

                match cmd {
                    DO => {
                        // Respond: IAC WONT <option> (refuse all)
                        let _ = self.stream.write_all(&[IAC, WONT, opt]);
                    }
                    DONT => {
                        let _ = self.stream.write_all(&[IAC, WONT, opt]);
                    }
                    WILL => {
                        // Respond: IAC DONT <option>
                        let _ = self.stream.write_all(&[IAC, DONT, opt]);
                    }
                    WONT => {
                        // Acknowledge
                    }
                    _ => {}
                }
                let _ = self.stream.flush();
                i += 3;
            } else if data[i] == IAC && i + 1 < data.len() {
                // Escaped IAC or incomplete sequence
                i += 2;
            } else {
                text.push(data[i]);
                i += 1;
            }
        }

        text
    }

    /// Read from the stream until we detect a Renode prompt or timeout.
    fn read_until_prompt(&mut self, timeout: Duration) -> String {
        let start = std::time::Instant::now();
        let mut text = String::new();
        let mut buf = [0u8; 4096];

        self.stream
            .set_read_timeout(Some(Duration::from_millis(500)))
            .ok();

        loop {
            if start.elapsed() > timeout {
                tracing::warn!("Timeout waiting for Renode prompt ({:?})", timeout);
                break;
            }
            match self.stream.read(&mut buf) {
                Ok(0) => break,
                Ok(n) => {
                    // Strip IAC sequences from data
                    let clean_bytes = self.handle_telnet_negotiation(&buf[..n]);
                    let chunk = String::from_utf8_lossy(&clean_bytes);
                    text.push_str(&chunk);

                    // Check for prompt
                    let stripped = strip_ansi(&text);
                    if let Some(last_line) = stripped.trim_end().lines().last() {
                        if is_prompt(last_line.trim()) {
                            break;
                        }
                    }
                }
                Err(e)
                    if e.kind() == std::io::ErrorKind::TimedOut
                        || e.kind() == std::io::ErrorKind::WouldBlock =>
                {
                    // On timeout, check if we already have a prompt
                    let stripped = strip_ansi(&text);
                    if let Some(last_line) = stripped.trim_end().lines().last() {
                        if is_prompt(last_line.trim()) {
                            break;
                        }
                    }
                    continue;
                }
                Err(_) => break,
            }
        }

        self.stream
            .set_read_timeout(Some(Duration::from_secs(10)))
            .ok();

        strip_ansi(&text)
    }

    /// Drain any pending data without blocking.
    fn drain_pending(&mut self) {
        self.stream
            .set_read_timeout(Some(Duration::from_millis(100)))
            .ok();
        let mut buf = [0u8; 4096];
        loop {
            match self.stream.read(&mut buf) {
                Ok(0) => break,
                Ok(_) => continue,
                Err(_) => break,
            }
        }
        self.stream
            .set_read_timeout(Some(Duration::from_secs(10)))
            .ok();
    }
}

/// Check if a line looks like a Renode prompt: "(MachineName)" or "(monitor)"
fn is_prompt(s: &str) -> bool {
    s.starts_with('(') && s.ends_with(')')
}

/// Strip ANSI escape sequences from a string.
fn strip_ansi(s: &str) -> String {
    let mut result = String::with_capacity(s.len());
    let mut in_escape = false;
    for c in s.chars() {
        if in_escape {
            if c.is_ascii_alphabetic() {
                in_escape = false;
            }
        } else if c == '\x1b' {
            in_escape = true;
        } else if !c.is_control() || c == '\n' {
            result.push(c);
        }
    }
    result
}
