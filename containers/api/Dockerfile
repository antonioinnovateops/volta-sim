FROM python:3.12-slim

WORKDIR /opt/volta

# Install dependencies
COPY api/requirements.txt /opt/volta/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy proto and generate stubs
COPY bridge/proto/ /opt/volta/bridge/proto/
COPY api/ /opt/volta/api/

RUN python -m grpc_tools.protoc \
    -I/opt/volta/bridge/proto \
    --python_out=/opt/volta/api/services \
    --grpc_python_out=/opt/volta/api/services \
    /opt/volta/bridge/proto/volta.proto \
    && sed -i 's/import volta_pb2/from api.services import volta_pb2/' \
       /opt/volta/api/services/volta_pb2_grpc.py

# Create firmware upload directory
RUN mkdir -p /opt/volta/firmware/uploads

EXPOSE 8080

CMD ["uvicorn", "api.main:app", "--host", "0.0.0.0", "--port", "8080"]
