#!/bin/bash
set -euo pipefail

# Resolve repository root based on script location to avoid hard-coded paths.
SCRIPT_DIR="$(cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== SERVER TERMINAL ==="
echo "Starting Dilithium PQC DTLS 1.3 Server..."
./host/server
