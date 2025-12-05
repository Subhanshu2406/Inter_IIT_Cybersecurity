#!/bin/bash
set -euo pipefail

# Resolve repository root based on script location to avoid hard-coded paths.
SCRIPT_DIR="$(cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

source "$SCRIPT_DIR/litex-env/bin/activate"

echo "=== CLIENT TERMINAL ==="
echo "Starting LiteX simulation with DTLS client..."

# Run with sudo while preserving the virtual environment's Python
sudo -E PATH="$PATH" PYTHONPATH="$PYTHONPATH" \
    "$SCRIPT_DIR/litex-env/bin/python3" \
    "$SCRIPT_DIR/litex-env/bin/litex_sim" \
    --csr-json csr.json \
    --cpu-type=vexriscv \
    --cpu-variant=full \
    --integrated-main-ram-size=0x06400000 \
    --ram-init=boot.bin \
    --with-ethernet 2>&1
