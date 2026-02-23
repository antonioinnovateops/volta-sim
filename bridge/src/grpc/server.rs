use std::sync::{Arc, Mutex};
use tonic::{Request, Response, Status};

use crate::renode::RenodeTelnet;

pub mod volta_proto {
    tonic::include_proto!("volta");
}

use volta_proto::volta_control_server::{VoltaControl, VoltaControlServer};
use volta_proto::*;

pub struct VoltaControlService {
    renode: Arc<Mutex<RenodeTelnet>>,
}

impl VoltaControlService {
    pub fn new(renode: Arc<Mutex<RenodeTelnet>>) -> Self {
        Self { renode }
    }

    pub fn into_server(self) -> VoltaControlServer<Self> {
        VoltaControlServer::new(self)
    }

    fn renode_cmd(&self, cmd: &str) -> Result<String, Status> {
        let mut renode = self.renode.lock().map_err(|e| {
            Status::internal(format!("Renode lock poisoned: {e}"))
        })?;
        renode
            .send_command(cmd)
            .map_err(|e| Status::internal(format!("Renode command failed: {e}")))
    }
}

#[tonic::async_trait]
impl VoltaControl for VoltaControlService {
    async fn flash_firmware(
        &self,
        request: Request<FlashRequest>,
    ) -> Result<Response<FlashResponse>, Status> {
        let path = request.into_inner().elf_path;
        tracing::info!("gRPC: FlashFirmware path={path}");
        let resp = self.renode_cmd(&format!("sysbus LoadELF @{path}"))?;
        Ok(Response::new(FlashResponse {
            success: true,
            message: format!("Loaded: {resp}"),
        }))
    }

    async fn start_simulation(
        &self,
        _request: Request<StartRequest>,
    ) -> Result<Response<StatusResponse>, Status> {
        tracing::info!("gRPC: StartSimulation");
        self.renode_cmd("start")?;
        Ok(Response::new(StatusResponse {
            state: "running".into(),
            message: "Simulation started".into(),
        }))
    }

    async fn stop_simulation(
        &self,
        _request: Request<StopRequest>,
    ) -> Result<Response<StatusResponse>, Status> {
        tracing::info!("gRPC: StopSimulation");
        self.renode_cmd("pause")?;
        Ok(Response::new(StatusResponse {
            state: "paused".into(),
            message: "Simulation paused".into(),
        }))
    }

    async fn get_status(
        &self,
        _request: Request<StatusRequest>,
    ) -> Result<Response<StatusResponse>, Status> {
        Ok(Response::new(StatusResponse {
            state: "unknown".into(),
            message: "Status check not yet implemented".into(),
        }))
    }
}
