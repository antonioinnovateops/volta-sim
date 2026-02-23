use clap::Parser;

#[derive(Parser, Debug, Clone)]
#[command(name = "voltad", about = "Volta-Sim bridge daemon")]
pub struct Config {
    /// Path to shared memory region
    #[arg(long, default_value = "/dev/shm/volta_peripheral_state")]
    pub shm_path: String,

    /// Size of shared memory region in bytes
    #[arg(long, default_value_t = 65536)]
    pub shm_size: usize,

    /// ZMQ PUB socket bind address
    #[arg(long, default_value = "tcp://0.0.0.0:5555")]
    pub zmq_pub_addr: String,

    /// gRPC server listen address
    #[arg(long, default_value = "0.0.0.0:50051")]
    pub grpc_addr: String,

    /// Renode telnet monitor address
    #[arg(long, default_value = "127.0.0.1:1234")]
    pub renode_addr: String,

    /// Renode UART socket terminal address (USART2 output)
    #[arg(long, default_value = "127.0.0.1:4321")]
    pub uart_addr: String,

    /// SHM poll interval in microseconds
    #[arg(long, default_value_t = 100)]
    pub poll_interval_us: u64,
}
