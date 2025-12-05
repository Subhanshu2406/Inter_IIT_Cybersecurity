#!/bin/bash
set -e

# ==============================================================================
# PQC Certificate Generation Script for LiteX + wolfSSL
# Generates Dilithium (ML-DSA) Certificates and Keys for Client/Server
# ==============================================================================

WORK_DIR="host/pqc_cert_gen"
WOLFSSL_DIR="$WORK_DIR/wolfssl"
OUTPUT_DIR="host/certs"

echo "=== Setting up PQC Certificate Generation Environment ==="

# 1. Prepare Directories
mkdir -p $WORK_DIR
mkdir -p $OUTPUT_DIR

# 2. Clone wolfSSL (if not present)
if [ ! -d "$WOLFSSL_DIR" ]; then
    echo "[*] Cloning wolfSSL..."
    git clone https://github.com/wolfSSL/wolfssl.git $WOLFSSL_DIR
else
    echo "[*] wolfSSL already cloned."
fi

# 3. Build wolfSSL with PQC support and Command Line Tool
cd $WOLFSSL_DIR
if [ ! -f "src/wolfssl" ]; then
    echo "[*] Configuring wolfSSL..."
    # Enable Dilithium, Kyber, TLS 1.3, and the command-line tool (opensslextra)
    ./autogen.sh
    ./configure --enable-dilithium --enable-kyber --enable-tls13 \
                --enable-opensslextra --enable-wolfssl --disable-shared \
                --enable-static CFLAGS="-DWOLFSSL_DILITHIUM_LEVEL=3 -DWOLFSSL_KYBER_LEVEL=3"
    
    echo "[*] Building wolfSSL..."
    make -j$(nproc)
else
    echo "[*] wolfSSL already built."
fi

# Tool path
WOLFSSL_TOOL="./src/wolfssl"

# 4. Generate Certificates
echo "=== Generating Dilithium (Level 3) Certificates ==="
cd ../../../$OUTPUT_DIR

# Clean old certs
rm -f *.pem *.der *.h

# --- CA ---
echo "[1/3] Generating Root CA..."
# Generate Self-Signed CA Certificate with Dilithium Key
# Note: wolfSSL CLI might differ slightly from OpenSSL, but this is the standard mapping
$WOLFSSL_DIR/examples/client/client -? > /dev/null 2>&1 || true # Just to check if binary runs

# We will use a small C program to generate the certs because the CLI tool 
# support for PQC generation flags can vary by version.
# Writing a generator is safer.

cat > gen_certs.c <<EOF
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <stdio.h>

void write_file(const char* fname, const byte* data, int len) {
    FILE* f = fopen(fname, "wb");
    if(f) { fwrite(data, 1, len, f); fclose(f); }
    else { printf("Failed to write %s\n", fname); }
}

int make_cert(const char* name, const char* subject, int is_ca, 
              Cert* ca_cert, Rng* rng, 
              byte* key_buf, word32* key_sz,
              byte* cert_buf, word32* cert_sz) 
{
    Cert myCert;
    wc_InitCert(&myCert);

    strncpy(myCert.subject.commonName, subject, sizeof(myCert.subject.commonName));
    myCert.isCA = is_ca;
    myCert.sigType = CTC_DILITHIUM_3; 
    myCert.daysValid = 365;
    
    // Generate Key
    int ret = wc_MakeDilithiumKey(key_buf, key_sz, rng);
    if (ret != 0) { printf("MakeKey failed: %d\n", ret); return ret; }

    // Self-signed (CA) or Signed by CA
    if (is_ca) {
        strncpy(myCert.issuer.commonName, subject, sizeof(myCert.issuer.commonName));
        ret = wc_MakeCert(&myCert, cert_buf, *cert_sz, NULL, key_buf, *key_sz, rng);
    } else {
        // Copy issuer from CA
        strncpy(myCert.issuer.commonName, ca_cert->subject.commonName, sizeof(myCert.issuer.commonName));
        // Sign with CA's key (passed in ca_cert struct usually, but here we need the key separately)
        // For simplicity in this raw API, wc_MakeCert signs with the key provided in arguments.
        // So we need the CA's key here if we are not self-signing.
        // Wait, wc_MakeCert takes the signer's key.
        // So if not CA, we need CA's key.
        // Let's adjust the function signature.
        return -1; // See main loop
    }
    
    if (ret > 0) { *cert_sz = ret; return 0; }
    printf("MakeCert failed: %d\n", ret);
    return ret;
}

int main() {
    Rng rng;
    wc_InitRng(&rng);
    wolfSSL_Init();
    wolfSSL_Debugging_ON();

    byte ca_key[5000]; word32 ca_key_sz = sizeof(ca_key);
    byte ca_cert_der[5000]; word32 ca_cert_sz = sizeof(ca_cert_der);
    
    byte server_key[5000]; word32 server_key_sz = sizeof(server_key);
    byte server_cert_der[5000]; word32 server_cert_sz = sizeof(server_cert_der);

    byte client_key[5000]; word32 client_key_sz = sizeof(client_key);
    byte client_cert_der[5000]; word32 client_cert_sz = sizeof(client_cert_der);

    int ret;

    // 1. CA
    printf("Generating CA...\n");
    Cert caCert;
    wc_InitCert(&caCert);
    strncpy(caCert.subject.commonName, "PQC Root CA", CTC_NAME_SIZE);
    caCert.isCA = 1;
    caCert.sigType = CTC_DILITHIUM_3;
    caCert.daysValid = 3650;
    ret = wc_MakeDilithiumKey(ca_key, &ca_key_sz, &rng);
    if(ret != 0) return ret;
    strncpy(caCert.issuer.commonName, "PQC Root CA", CTC_NAME_SIZE);
    ret = wc_MakeCert(&caCert, ca_cert_der, ca_cert_sz, NULL, ca_key, ca_key_sz, &rng);
    if(ret < 0) return ret;
    ca_cert_sz = ret;
    write_file("ca_cert.der", ca_cert_der, ca_cert_sz);
    write_file("ca_key.der", ca_key, ca_key_sz);

    // 2. Server
    printf("Generating Server Cert...\n");
    Cert srvCert;
    wc_InitCert(&srvCert);
    strncpy(srvCert.subject.commonName, "PQC Server", CTC_NAME_SIZE);
    srvCert.isCA = 0;
    srvCert.sigType = CTC_DILITHIUM_3;
    srvCert.daysValid = 365;
    ret = wc_MakeDilithiumKey(server_key, &server_key_sz, &rng);
    if(ret != 0) return ret;
    
    // Sign with CA Key
    strncpy(srvCert.issuer.commonName, "PQC Root CA", CTC_NAME_SIZE);
    // wc_MakeCert signs the new cert (srvCert) using the provided key (ca_key)
    // The public key in the cert will be server_key (we need to set it)
    // Actually wc_MakeCert generates the key if not provided? No.
    // We need to set the public key of the subject.
    // wc_MakeCert_ex is better but complex.
    
    // EASIER WAY: Use the wolfSSL example tools which are already built.
    return 0;
}
EOF

# Revert to using the provided 'wolfssl' tool which mimics openssl.
# It is much easier than writing C code for cert gen.

echo "Using wolfSSL command line tool..."
TOOL=../../../$WOLFSSL_DIR/src/wolfssl

# 1. CA
# Generate CA Key (Dilithium3)
$TOOL genpqc key dilithium3 ca_key.der
# Generate CA Cert (Self-signed)
$TOOL x509 -new -key ca_key.der -out ca_cert.der -outform DER -subj "/CN=PQC Root CA" -days 3650

# 2. Server
# Generate Server Key
$TOOL genpqc key dilithium3 server_key.der
# Generate Server CSR
$TOOL req -new -key server_key.der -out server.csr -outform DER -subj "/CN=PQC Server"
# Sign Server Cert with CA
$TOOL x509 -req -in server.csr -inform DER -CA ca_cert.der -CAform DER -CAkey ca_key.der -CAkeyform DER -out server_cert.der -outform DER -days 365 -set_serial 01

# 3. Client
# Generate Client Key
$TOOL genpqc key dilithium3 client_key.der
# Generate Client CSR
$TOOL req -new -key client_key.der -out client.csr -outform DER -subj "/CN=PQC Client"
# Sign Client Cert with CA
$TOOL x509 -req -in client.csr -inform DER -CA ca_cert.der -CAform DER -CAkey ca_key.der -CAkeyform DER -out client_cert.der -outform DER -days 365 -set_serial 02

echo "=== Converting to C Arrays ==="

# Function to convert and format
convert_to_c() {
    local name=$1
    local file=$2
    echo "const unsigned char ${name}[] = {" > ${name}.h
    xxd -i < $file >> ${name}.h
    echo "};" >> ${name}.h
    # Fix the xxd output to be a valid C array body (remove the variable name xxd adds if any, but xxd -i < file just outputs bytes)
    # Actually xxd -i filename outputs "unsigned char filename[] = { ... };"
    # xxd -i < file outputs just the bytes "0x00, 0x01, ..."
    
    # Let's use xxd -i directly on file
    xxd -i $file > ${name}_temp.h
    # Extract just the body or keep it as is.
    # We want to copy-paste the body.
}

xxd -i ca_cert.der > ca_cert_def.h
xxd -i server_cert.der > server_cert_def.h
xxd -i server_key.der > server_key_def.h
xxd -i client_cert.der > client_cert_def.h
xxd -i client_key.der > client_key_def.h

echo "=== DONE ==="
echo "Certificates generated in $OUTPUT_DIR"
echo "Copy the contents of the *_def.h files into your C code."
