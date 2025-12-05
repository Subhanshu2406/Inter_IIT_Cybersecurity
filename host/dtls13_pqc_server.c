#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/error-ssl.h>

#define DEFAULT_BIND_IP   "192.168.1.100"
#define DEFAULT_BIND_PORT 6000
#define DTLS_MTU          1200

// ------------------------ PQC Certificates & Keys ------------------------
// NOTE: These are PLACEHOLDERS. You must generate valid Dilithium/Kyber
// certificates using wolfSSL tools and replace these arrays.

// Root CA Certificate (Dilithium Signed) - Same as client's CA
static const unsigned char ca_cert_der[] = {
    0x30, 0x82, 0x05, 0x5c, 0x30, 0x82, 0x03, 0x44, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01,
    // ... (Truncated - REPLACE WITH REAL DATA) ...
    0x00, 0x00, 0x00, 0x00
};

// Server Certificate (Dilithium Signed)
static const unsigned char server_cert_der[] = {
    0x30, 0x82, 0x05, 0x5c, 0x30, 0x82, 0x03, 0x44, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01,
    // ... (Truncated - REPLACE WITH REAL DATA) ...
    0x00, 0x00, 0x00, 0x00
};

// Server Private Key (Dilithium)
static const unsigned char server_key_der[] = {
    0x30, 0x82, 0x05, 0x5c, 0x30, 0x82, 0x03, 0x44, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01,
    // ... (Truncated - REPLACE WITH REAL DATA) ...
    0x00, 0x00, 0x00, 0x00
};

typedef struct {
    int sock;
    struct sockaddr_in peer;
    socklen_t peer_len;
    int peer_set;
} net_ctx_t;

static int bio_recv(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    (void)ssl;
    net_ctx_t* net = (net_ctx_t*)ctx;
    if (net == NULL)
        return WOLFSSL_CBIO_ERR_GENERAL;

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int got = (int)recvfrom(net->sock, buf, (size_t)sz, 0,
                            (struct sockaddr*)&from, &from_len);
    if (got < 0) {
        if (errno == EINTR || errno == EAGAIN)
            return WOLFSSL_CBIO_ERR_WANT_READ;
        return WOLFSSL_CBIO_ERR_GENERAL;
    }

    if (!net->peer_set) {
        net->peer = from;
        net->peer_len = from_len;
        net->peer_set = 1;
        
        char addr[32];
        inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));
        printf("[UDP] New peer detected: %s:%u\n", addr, ntohs(from.sin_port));
    }

    return got;
}

static int bio_send(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    (void)ssl;
    net_ctx_t* net = (net_ctx_t*)ctx;
    if (net == NULL || !net->peer_set)
        return WOLFSSL_CBIO_ERR_GENERAL;

    int sent = (int)sendto(net->sock, buf, (size_t)sz, 0,
                           (struct sockaddr*)&net->peer, net->peer_len);
    return (sent >= 0) ? sent : WOLFSSL_CBIO_ERR_GENERAL;
}

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    int ret;

    printf("Starting PQC-DTLS 1.3 Server (Kyber + Dilithium)...\n");

    // 1. Setup UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(DEFAULT_BIND_PORT);
    bind_addr.sin_addr.s_addr = inet_addr(DEFAULT_BIND_IP);

    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }
    printf("Listening on %s:%d\n", DEFAULT_BIND_IP, DEFAULT_BIND_PORT);

    // 2. wolfSSL Init
    wolfSSL_Init();
    wolfSSL_Debugging_ON();

    WOLFSSL_CTX* ctx = wolfSSL_CTX_new(wolfDTLSv1_3_server_method());
    if (ctx == NULL) {
        printf("wolfSSL_CTX_new failed\n");
        close(sock);
        return 1;
    }

    // 3. Load Certificates & Keys
    if (wolfSSL_CTX_load_verify_buffer(ctx, ca_cert_der, sizeof(ca_cert_der), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
        printf("Failed to load CA certificate\n");
    }
    if (wolfSSL_CTX_use_certificate_buffer(ctx, server_cert_der, sizeof(server_cert_der), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
        printf("Failed to load Server certificate\n");
    }
    if (wolfSSL_CTX_use_PrivateKey_buffer(ctx, server_key_der, sizeof(server_key_der), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
        printf("Failed to load Server private key\n");
    }

    // 4. Require Mutual Authentication
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER | WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);

    // 5. Set Cipher Suite & Groups
    wolfSSL_CTX_set_cipher_list(ctx, "TLS13-AES128-GCM-SHA256");
    if (wolfSSL_CTX_set_groups_list(ctx, "ML-KEM-768") != WOLFSSL_SUCCESS) {
        printf("Failed to set PQC groups (Kyber)\n");
    }

    // 6. Set IO Callbacks
    wolfSSL_SetIORecv(ctx, bio_recv);
    wolfSSL_SetIOSend(ctx, bio_send);
    
    // Fix MTU
    wolfSSL_CTX_set_options(ctx, WOLFSSL_OP_NO_QUERY_MTU);

    // 7. Accept Loop
    while (1) {
        net_ctx_t net;
        memset(&net, 0, sizeof(net));
        net.sock = sock;
        net.peer_set = 0;

        WOLFSSL* ssl = wolfSSL_new(ctx);
        if (ssl == NULL) {
            printf("wolfSSL_new failed\n");
            break;
        }
        
        wolfSSL_dtls_set_mtu(ssl, DTLS_MTU);
        wolfSSL_SetIOReadCtx(ssl, &net);
        wolfSSL_SetIOWriteCtx(ssl, &net);

        printf("Waiting for client...\n");
        
        // In a real server, we'd peek at the first packet to identify the peer before creating SSL
        // For this demo, we just block on the first read inside wolfSSL_accept via bio_recv
        
        ret = wolfSSL_accept(ssl);
        if (ret != WOLFSSL_SUCCESS) {
            int err = wolfSSL_get_error(ssl, ret);
            printf("wolfSSL_accept failed: %d\n", err);
            wolfSSL_free(ssl);
            continue;
        }

        printf("Handshake complete! Cipher: %s\n", wolfSSL_get_cipher(ssl));

        // Read message
        char buf[256];
        ret = wolfSSL_read(ssl, buf, sizeof(buf)-1);
        if (ret > 0) {
            buf[ret] = 0;
            printf("Received: %s\n", buf);
            
            // Echo back
            wolfSSL_write(ssl, buf, ret);
        }

        wolfSSL_free(ssl);
        printf("Session closed.\n");
    }

    wolfSSL_CTX_free(ctx);
    wolfSSL_Cleanup();
    close(sock);
    return 0;
}
