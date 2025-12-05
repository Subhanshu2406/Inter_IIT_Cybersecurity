#!/bin/bash
# Dilithium certificate generator using P-256 ECC (prime256v1)
# P-256 has better support in embedded wolfSSL

set -e

CERT_DIR="host/certs_dilithium"

echo "=== Generating Dilithium (P-256 ECC placeholder) Certificates ==="

# Create directory
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

# Clean old certificates
rm -f *.pem *.der *.csr *.srl

echo "[1/3] Generating CA Certificate (P-256)..."
openssl ecparam -name prime256v1 -genkey -noout -out ca-key.pem
openssl req -new -x509 -key ca-key.pem -out ca-cert.pem -days 3650 \
    -subj "/C=US/ST=State/L=City/O=LiteX-PQC/OU=CA/CN=LiteX Dilithium CA"

echo "[2/3] Generating Server Certificate (P-256)..."
openssl ecparam -name prime256v1 -genkey -noout -out server-key.pem
openssl req -new -key server-key.pem -out server.csr \
    -subj "/C=US/ST=State/L=City/O=LiteX-PQC/OU=Server/CN=LiteX Dilithium Server"
openssl x509 -req -in server.csr -CA ca-cert.pem -CAkey ca-key.pem \
    -CAcreateserial -out server-cert.pem -days 365

echo "[3/3] Generating Client Certificate (P-256)..."
openssl ecparam -name prime256v1 -genkey -noout -out client-key.pem
openssl req -new -key client-key.pem -out client.csr \
    -subj "/C=US/ST=State/L=City/O=LiteX-PQC/OU=Client/CN=LiteX Dilithium Client"
openssl x509 -req -in client.csr -CA ca-cert.pem -CAkey ca-key.pem \
    -CAcreateserial -out client-cert.pem -days 365

# Convert to DER format
echo "[4/4] Converting to DER format..."
openssl x509 -in ca-cert.pem -outform DER -out ca-cert.der
openssl x509 -in server-cert.pem -outform DER -out server-cert.der
openssl x509 -in client-cert.pem -outform DER -out client-cert.der
openssl ec -in server-key.pem -outform DER -out server-key.der
openssl ec -in client-key.pem -outform DER -out client-key.der

echo ""
echo "=== Certificate Generation Complete ==="
echo "Certificates stored in: $CERT_DIR"
echo ""
echo "Generated files:"
ls -lh *.pem *.der
echo ""
echo "Certificate details:"
openssl x509 -in ca-cert.pem -noout -subject -issuer -dates
