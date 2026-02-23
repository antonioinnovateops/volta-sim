use anyhow::{Context, Result};
use std::io::Read;
use std::net::TcpStream;
use std::time::Duration;

/// Reads UART output from Renode's ServerSocketTerminal via TCP.
pub struct UartSocketReader {
    stream: TcpStream,
}

impl UartSocketReader {
    pub fn connect(addr: &str) -> Result<Self> {
        let stream = TcpStream::connect(addr)
            .with_context(|| format!("Failed to connect to UART terminal at {addr}"))?;
        stream
            .set_read_timeout(Some(Duration::from_millis(50)))
            .ok();
        Ok(Self { stream })
    }

    pub fn connect_with_retry(addr: &str, max_attempts: u32) -> Result<Self> {
        for attempt in 1..=max_attempts {
            match Self::connect(addr) {
                Ok(reader) => return Ok(reader),
                Err(e) => {
                    if attempt == max_attempts {
                        return Err(e);
                    }
                    tracing::debug!(
                        "UART terminal not ready (attempt {attempt}/{max_attempts}): {e}"
                    );
                    std::thread::sleep(Duration::from_secs(1));
                }
            }
        }
        unreachable!()
    }

    /// Read available bytes. Returns None if connection closed.
    pub fn read_available(&mut self) -> Option<Vec<u8>> {
        let mut buf = [0u8; 1024];
        match self.stream.read(&mut buf) {
            Ok(0) => None,
            Ok(n) => Some(buf[..n].to_vec()),
            Err(e)
                if e.kind() == std::io::ErrorKind::TimedOut
                    || e.kind() == std::io::ErrorKind::WouldBlock =>
            {
                Some(Vec::new())
            }
            Err(_) => None,
        }
    }
}
