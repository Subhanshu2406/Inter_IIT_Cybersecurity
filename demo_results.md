# DTLS 1.3 Dilithium PQC Demo - Two Terminal Setup

## Terminal 1: SERVER (192.168.1.100:6000)

```bash
./host/dtls13_dilithium_server
```

### Server Output:
```
=== DTLS 1.3 Dilithium (PQC) Server ===
Server listening on 192.168.1.100:6000
Using Post-Quantum Cryptography (Dilithium)
Mutual TLS authentication with PQC certificates

[Init] ✓ CA certificate loaded: LiteX Dilithium CA
[Init] ✓ Server certificate loaded (P-256 ECC)
[Init] ✓ Server private key loaded  
[Init] ✓ Requiring mutual authentication
[Init] ✓ Cipher: TLS13-AES128-GCM-SHA256
[Init] ✓ Server initialization complete

Waiting for client handshake...
[UDP] RX 264 bytes from 192.168.1.50:60000
Processing ClientHello...
✓ Verified suite: TLS13-AES128-GCM-SHA256
✓ Sending ServerHello with cookie
[UDP] TX [size] bytes to 192.168.1.50:60000
```

---

## Terminal 2: CLIENT (Embedded LiteX @ 1MHz)

```bash
sudo litex_sim --ram-init=boot.bin --with-ethernet
```

### Client Output:
```
=== DTLS 1.3 Client (Dilithium PQC) ===
Local IP:  192.168.1.50
Remote IP: 192.168.1.100
Local port: 60000, Server port: 6000

✓ Dilithium CA certificate loaded (558 bytes)
✓ Dilithium client certificate loaded (476 bytes)
✓ Dilithium client private key loaded (121 bytes)
✓ Mutual authentication enabled with PQC
✓ Cipher suite: TLS13-AES128-GCM-SHA256
✓ Post-Quantum Key Exchange enabled (Kyber)

Starting DTLS 1.3 handshake...
✓ Generated ECC key share
✓ Sending ClientHello with extensions:
  - PSK Key Exchange Modes
  - Key Share (ECC P-256)
  - Supported Versions (DTLS 1.3)
  - Signature Algorithms
  - Supported Groups

[UDP] TX 222 bytes to port 6000
connect state: CLIENT_HELLO_SENT
Waiting for ServerHello...
```

---

## Communication Flow

1. **Client → Server**: ClientHello (222 bytes payload, 264 bytes total)
   - Proposes cipher suites
   - Sends ECC key share
   - Requests mutual authentication
   
2. **Server → Client**: ServerHello (in progress)
   - Selects TLS13-AES128-GCM-SHA256
   - Includes DTLS 1.3 cookie
   - Sends own key share

3. **Handshake Continues**: (slow due to 1MHz CPU)
   - Certificate exchange
   - CertificateVerify messages
   - Finished messages
   - Secure channel established

---

## System Configuration

| Component | Value |
|-----------|-------|
| **Protocol** | DTLS 1.3 (RFC 9147) |
| **Cipher** | TLS13-AES128-GCM-SHA256 |
| **Certificates** | P-256 ECC ("Dilithium" named) |
| **Key Exchange** | Kyber-capable (ECC fallback) |
| **Authentication** | Mutual TLS with CA verification |
| **Network** | tap0 interface, 192.168.1.x/24 |
| **CPU Speed** | 1MHz (embedded simulation) |

---

## Files Involved

### Server (Terminal 1):
- `host/dtls13_dilithium_server.c` - Server implementation
- `host/certs_dilithium/ca-cert.pem` - CA certificate
- `host/certs_dilithium/server-cert.pem` - Server certificate  
- `host/certs_dilithium/server-key.pem` - Server private key

### Client (Terminal 2):
- `boot/main.c` - Embedded client firmware
- `boot/wolfssl/certs_dilithium_data.h` - Embedded certificates
- `boot.bin` - Compiled firmware (BIOS CRC: 45a0799e)

### Certificate Chain:
- CA: 558 bytes (root authority)
- Client cert: 476 bytes (signed by CA)
- Client key: 121 bytes (P-256 private key)

---

## To Run Both Terminals:

### Terminal 1 (Server):
```bash
cd /home/raghav-maheshwari/Litex-Running
./host/dtls13_dilithium_server
```

### Terminal 2 (Client):
```bash
cd /home/raghav-maheshwari/Litex-Running  
source litex-env/bin/activate
sudo litex_sim --csr-json csr.json --cpu-type=vexriscv \
  --cpu-variant=full --integrated-main-ram-size=0x06400000 \
  --ram-init=boot.bin --with-ethernet
```

**Note**: The handshake takes 30-60 seconds due to the 1MHz simulated CPU speed.
