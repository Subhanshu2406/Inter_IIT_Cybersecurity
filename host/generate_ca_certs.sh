#!/bin/bash
# ==============================================================================
# CA Certificate Generation Script for DTLS 1.3 with wolfSSL
# Generates RSA/ECC certificates for mutual TLS authentication
# ==============================================================================

set -e

CERT_DIR="host/certs"
CA_DAYS=3650
CERT_DAYS=365

echo "=== Generating CA Certificates for DTLS 1.3 ==="

# Create certificate directory
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

# Clean old certificates
rm -f *.pem *.der *.srl *.key *.csr *.crt

echo "[1/3] Generating Root CA..."

# Generate CA private key (ECC P-256 for better performance on embedded)
openssl ecparam -name prime256v1 -genkey -noout -out ca-key.pem

# Generate self-signed CA certificate
openssl req -new -x509 -days $CA_DAYS -key ca-key.pem -out ca-cert.pem \
    -subj "/C=US/ST=State/L=City/O=LiteX/OU=CA/CN=LiteX Root CA"

echo "[2/3] Generating Server Certificate..."

# Generate server private key
openssl ecparam -name prime256v1 -genkey -noout -out server-key.pem

# Generate server certificate signing request
openssl req -new -key server-key.pem -out server.csr \
    -subj "/C=US/ST=State/L=City/O=LiteX/OU=Server/CN=LiteX DTLS Server"

# Sign server certificate with CA
openssl x509 -req -in server.csr -CA ca-cert.pem -CAkey ca-key.pem \
    -CAcreateserial -out server-cert.pem -days $CERT_DAYS

echo "[3/3] Generating Client Certificate..."

# Generate client private key
openssl ecparam -name prime256v1 -genkey -noout -out client-key.pem

# Generate client certificate signing request
openssl req -new -key client-key.pem -out client.csr \
    -subj "/C=US/ST=State/L=City/O=LiteX/OU=Client/CN=LiteX DTLS Client"

# Sign client certificate with CA
openssl x509 -req -in client.csr -CA ca-cert.pem -CAkey ca-key.pem \
    -CAcreateserial -out client-cert.pem -days $CERT_DAYS

# Convert to DER format for embedding
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
echo "Files generated:"
ls -lh *.pem *.der
echo ""
echo "CA Certificate:"
openssl x509 -in ca-cert.pem -noout -subject -issuer -dates
echo ""
echo "Server Certificate:"
openssl x509 -in server-cert.pem -noout -subject -issuer -dates
echo ""
echo "Client Certificate:"
openssl x509 -in client-cert.pem -noout -subject -issuer -dates
