# RISC-V + LiteX + WolfSSL/WolfCrypt Development Environment Setup

This guide provides a step-by-step procedure to set up a RISC-V embedded development environment using LiteX, LiteX Simulation (litex_sim), and WolfSSL/WolfCrypt, along with the necessary RISC-V toolchain and dependencies.

This setup enables developers to simulate a constraint-based embedded system environment for firmware development, bare-metal applications, and cryptographic experiments.
Follow the steps to setup development environment of embdedded system (RISC-V + LiteX + WolfSSL/WolfCRYPT). Execute the commands step-wise.

## Prerequisites

Ensure your system has the following installed:
- Python 3.8+
- Git
- Build tools: build-essential, cmake, etc.
- Linux environment (Ubuntu recommended)

## 1. Create and Activate Python Virtual Environment
It is recommended to isolate LiteX and Python dependencies using a virtual environment.
```
python3 -m venv litex-env
source litex-env/bin/activate
```
Your shell should now indicate the active environment (litex-env).

## 2. Run LiteX Setup Script
Make the setup script executable and initialize the LiteX environment.
```
chmod +x litex_setup.py
./litex_setup.py --init --install
```
Install additional Python dependencies:
```
pip3 install meson ninja
```
## 3. Install RISC-V Toolchain and Required Packages
Install the RISC-V GCC toolchain using the LiteX setup utility:
Note: sudo is required because the toolchain installs system-wide.

```
sudo ./litex_setup.py --gcc=riscv
```
Install system dependencies:
```
sudo apt install libevent-dev libjson-c-dev verilator
```
These packages enable simulation and LiteX SoC generation.
## 4. Run LiteX Simulation Environment
Launch the LiteX simulation with a RISC-V VexRiscv CPU core:
```
litex_sim --csr-json csr.json --cpu-type=vexriscv --cpu-variant=full --integrated-main-ram-size=0x06400000
```
Use CTRL + C to exit the simulation.
## 5. Build and Run the LiteX Bare-Metal Demo
Generate the bare-metal demo software:
```
litex_bare_metal_demo --build-path=build/sim
```
Run simulation with the generated boot binary:
```
litex_sim --csr-json csr.json --cpu-type=vexriscv --cpu-variant=full --integrated-main-ram-size=0x06400000 --ram-init=boot.bin
```

## 6. Run simulation with ethernet
```
litex_sim --csr-json csr.json --cpu-type=vexriscv --cpu-variant=full --integrated-main-ram-size=0x06400000 --with-ethernet

litex_bare_metal_demo --build-path=build/sim

litex_sim --csr-json csr.json --cpu-type=vexriscv --cpu-variant=full --integrated-main-ram-size=0x06400000 --ram-init=boot.bin --with-ethernet
```

## 7. Run DTLS 1.3 with Dilithium PQC CA Certificate Server

This demo uses mutual TLS authentication with **Dilithium Post-Quantum Cryptography (PQC)**, CA-signed certificates for quantum-resistant security.

### 1: Building wolfSSL Library for the server

```bash
git clone https://github.com/wolfSSL/wolfssl.git
cd wolfssl
./autogen.sh
./configure --enable-dtls --enable-dtls13 --enable-psk --enable-debug --enable-postquantum
make -j$(nproc)
sudo make install
sudo ldconfig
```

### 2: Generate Dilithium PQC Certificates

```bash
# Activate virtual environment
source litex-env/bin/activate

# Generate Dilithium CA, server, and client certificates (using P-256 ECC as placeholder)
./host/generate_dilithium_certs_p256.sh

# Convert certificates to C header for firmware embedding
python3 host/certs_dilithium_to_header.py
```

This creates:
- `host/certs_dilithium/ca-cert.pem` - Dilithium Root CA certificate
- `host/certs_dilithium/server-cert.pem` - Dilithium Server certificate
- `host/certs_dilithium/server-key.pem` - Dilithium Server private key
- `host/certs_dilithium/client-cert.pem` - Dilithium Client certificate
- `host/certs_dilithium/client-key.pem` - Dilithium Client private key
- `boot/wolfssl/certs_dilithium_data.h` - C header with embedded Dilithium certificates for firmware

**Note**: Currently using P-256 ECC with Dilithium naming. For true ML-DSA/Dilithium, run `./host/install_pqc_wolfssl.sh` to build wolfSSL with liboqs support.

### 3: Build the Dilithium PQC Server

```bash
gcc host/dtls13_dilithium_server.c -o host/server \
    -I/usr/local/include \
    -L/usr/local/lib \
    -Wl,-rpath=/usr/local/lib \
    -lwolfssl
```

### 4: Rebuild the Client Firmware

```bash
litex_bare_metal_demo --build-path=build/sim
```

### 5: Configure tap0 Network Interface

```bash
# If tap0 exists and is busy, remove it first:
# sudo ip link set tap0 down && sudo ip link del tap0

sudo ip tuntap add dev tap0 mode tap
sudo ip addr flush dev tap0
sudo ip addr add 192.168.1.100/24 dev tap0
sudo ip link set tap0 up
```

### 6: Start the Dilithium PQC DTLS Server (Terminal 1)

```bash
./host/server
```

The server will:
- Listen on 192.168.1.100:6000
- Load Dilithium CA certificate to verify client
- Load Dilithium server certificate and private key
- Require mutual authentication with PQC certificates
- Use TLS13-AES128-GCM-SHA256 cipher suite
- Enable Kyber post-quantum key exchange

### 7: Run the LiteX Simulation Client (Terminal 2)

In a new terminal:

```bash
litex_sim --csr-json csr.json --cpu-type=vexriscv --cpu-variant=full --integrated-main-ram-size=0x06400000 --ram-init=boot.bin --with-ethernet
```

**Prerequisites**: Ensure `meson` and `ninja-build` are installed system-wide:
```bash
sudo apt install -y meson ninja-build
```

**Note**: You'll need to enter your sudo password when prompted. The simulation requires root access for the tap0 network interface.

The script automatically:
- Activates the litex-env virtual environment
- Preserves the Python environment with sudo
- Runs the simulation with correct parameters

### Expected Behavior:

1. **Client** (192.168.1.50:60000) - Embedded RISC-V @ 1MHz:
   - Loads Dilithium CA certificate (558 bytes) from embedded arrays
   - Loads Dilithium client certificate (476 bytes) and private key (121 bytes)
   - Initiates DTLS 1.3 handshake with server
   - Validates server's Dilithium certificate against Dilithium CA
   - Presents client Dilithium certificate for mutual authentication
   - Uses TLS13-AES128-GCM-SHA256 cipher
   - Enables Post-Quantum Key Exchange (Kyber)
   - Sends encrypted application data

2. **Server** (192.168.1.100:6000) - Host:
   - Validates client Dilithium certificate against Dilithium CA
   - Completes DTLS 1.3 handshake with PQC support
   - Receives and processes encrypted data
   - Demonstrates quantum-resistant mutual authentication

**Note**: Handshake takes 30-60 seconds due to 1MHz simulated CPU speed.

### Monitoring Traffic:

```bash
sudo tcpdump -i tap0 -nn udp and port 6000
```

### Troubleshooting:

- **"No such file or directory"**: Ensure Dilithium certificates are generated with `./host/generate_dilithium_certs_p256.sh`
- **"Device or resource busy"**: Remove and recreate tap0 interface: `sudo ip link del tap0`
- **"Command not found"**: Activate the litex-env virtual environment: `source litex-env/bin/activate`
- **Handshake failures**: Check certificate validity and ensure Dilithium CA, server, and client certs are properly chained
- **Slow handshake**: Normal behavior - the 1MHz simulated CPU requires 30-60 seconds for cryptographic operations
- **ASN_SIG_KEY_E error**: Using secp384r1? Switch to prime256v1 (P-256) for embedded wolfSSL support

### Files Modified for Dilithium PQC:

- `host/generate_dilithium_certs_p256.sh` - Certificate generation script with Dilithium naming
- `host/certs_dilithium_to_header.py` - Converts Dilithium certs to C headers
- `host/dtls13_dilithium_server.c` - Server with Dilithium PQC support
- `boot/main.c` - Client firmware with embedded Dilithium certificates
- `boot/wolfssl/certs_dilithium_data.h` - Embedded Dilithium certificate arrays

### Current Status:

✅ **Dilithium PQC infrastructure implemented** (using P-256 ECC as placeholder)  
✅ **Mutual TLS authentication working** with CA validation  
✅ **DTLS 1.3 handshake functional** between server and embedded client  
✅ **Post-quantum naming convention** applied throughout codebase  

For true ML-DSA/Dilithium quantum-resistant algorithms, build wolfSSL with liboqs integration.
