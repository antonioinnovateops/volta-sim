#!/usr/bin/env bash
# Generate Python gRPC stubs from volta.proto
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROTO_DIR="$(cd "$SCRIPT_DIR/../../bridge/proto" && pwd)"
OUT_DIR="$SCRIPT_DIR"

python -m grpc_tools.protoc \
    -I"$PROTO_DIR" \
    --python_out="$OUT_DIR" \
    --grpc_python_out="$OUT_DIR" \
    "$PROTO_DIR/volta.proto"

# Fix import path in generated grpc file
sed -i 's/import volta_pb2/from api.services import volta_pb2/' "$OUT_DIR/volta_pb2_grpc.py"

echo "Generated: $OUT_DIR/volta_pb2.py, $OUT_DIR/volta_pb2_grpc.py"
