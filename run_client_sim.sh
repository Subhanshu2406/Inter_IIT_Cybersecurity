#!/bin/bash
cd /home/raghav-maheshwari/Litex-Running
source litex-env/bin/activate
echo "=== CLIENT TERMINAL ==="
echo "Starting LiteX simulation with DTLS client..."
# Run with sudo while preserving the virtual environment's Python
sudo -E PATH="$PATH" PYTHONPATH="$PYTHONPATH" \
    "$PWD/litex-env/bin/python3" \
    "$PWD/litex-env/bin/litex_sim" \
    --csr-json csr.json \
    --cpu-type=vexriscv \
    --cpu-variant=full \
    --integrated-main-ram-size=0x06400000 \
    --ram-init=boot.bin \
    --with-ethernet 2>&1
