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

#define CA_CERT_FILE      "host/certs/ca-cert.pem"
#define SERVER_CERT_FILE  "host/certs/server-cert.pem"
#define SERVER_KEY_FILE   "host/certs/server-key.pem"

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
    }

    char addr[32];
    inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));
    printf("[UDP] RX %d bytes from %s:%u\n", got, addr, ntohs(from.sin_port));
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
    (void)argc;
    (void)argv;

    const char* bind_ip = DEFAULT_BIND_IP;
    int bind_port = DEFAULT_BIND_PORT;

    net_ctx_t net;
    memset(&net, 0, sizeof(net));
    net.sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (net.sock < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(net.sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(net.sock);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)bind_port);
    if (inet_aton(bind_ip, &addr.sin_addr) == 0) {
        fprintf(stderr, "Invalid bind IP\n");
        close(net.sock);
        return 1;
    }

    if (bind(net.sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(net.sock);
        return 1;
    }
    
    printf("\n=== DTLS 1.3 CA Certificate Server ===\n");
    printf("Server listening on %s:%d\n", bind_ip, bind_port);
    printf("Using mutual TLS authentication with certificates\n");
    printf("Waiting for client connection...\n\n");

    printf("[Init] Initializing wolfSSL library...\n");
    wolfSSL_Init();
    wolfSSL_Debugging_ON();

    printf("[Init] Creating DTLS 1.3 server context...\n");
    WOLFSSL_CTX* ctx = wolfSSL_CTX_new(wolfDTLSv1_3_server_method());
    if (ctx == NULL) {
        fprintf(stderr, "[Init] ✗ wolfSSL_CTX_new failed\n");
        close(net.sock);
        return 1;
    }
    
    printf("[Init] Loading CA certificate: %s\n", CA_CERT_FILE);
    if (wolfSSL_CTX_load_verify_locations(ctx, CA_CERT_FILE, NULL) != WOLFSSL_SUCCESS) {
        fprintf(stderr, "[Init] ✗ Failed to load CA certificate\n");
        wolfSSL_CTX_free(ctx);
        close(net.sock);
        return 1;
    }
    
    printf("[Init] Loading server certificate: %s\n", SERVER_CERT_FILE);
    if (wolfSSL_CTX_use_certificate_file(ctx, SERVER_CERT_FILE, WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
        fprintf(stderr, "[Init] ✗ Failed to load server certificate\n");
        wolfSSL_CTX_free(ctx);
        close(net.sock);
        return 1;
    }
    
    printf("[Init] Loading server private key: %s\n", SERVER_KEY_FILE);
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, SERVER_KEY_FILE, WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
        fprintf(stderr, "[Init] ✗ Failed to load server private key\n");
        wolfSSL_CTX_free(ctx);
        close(net.sock);
        return 1;
    }
    
    printf("[Init] Requiring mutual authentication (client certificate)...\n");
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER | WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    
    printf("[Init] Setting cipher suite to TLS13-AES128-GCM-SHA256...\n");
    wolfSSL_CTX_set_cipher_list(ctx, "TLS13-AES128-GCM-SHA256");
    
    printf("[Init] Setting custom I/O callbacks...\n");
    wolfSSL_SetIORecv(ctx, bio_recv);
    wolfSSL_SetIOSend(ctx, bio_send);

    printf("[Init] Creating SSL session object...\n");
    WOLFSSL* ssl = wolfSSL_new(ctx);
    if (ssl == NULL) {
        fprintf(stderr, "[Init] ✗ wolfSSL_new failed\n");
        wolfSSL_CTX_free(ctx);
        close(net.sock);
        return 1;
    }

    printf("[Init] Configuring MTU settings...\n");
    wolfSSL_CTX_set_options(ctx, WOLFSSL_OP_NO_QUERY_MTU);
    printf("[Init] ✓ Server initialization complete\n");

    wolfSSL_SetIOReadCtx(ssl, &net);
    wolfSSL_SetIOWriteCtx(ssl, &net);
    
    printf("\n=== Starting DTLS 1.3 Handshake ===\n");
    printf("Waiting for client handshake...\n");
    int ret;
    int handshake_attempts = 0;
    for (;;) {
        handshake_attempts++;
        printf("[Handshake] Attempt #%d: Calling wolfSSL_accept()...\n", handshake_attempts);
        ret = wolfSSL_accept(ssl);
        if (ret == WOLFSSL_SUCCESS) {
            printf("[Handshake] ✓ SUCCESS after %d attempts\n", handshake_attempts);
            break;
        }
        int err = wolfSSL_get_error(ssl, ret);
        if (err == WOLFSSL_ERROR_WANT_READ) {
            printf("[Handshake] Need more data (WANT_READ)\n");
            continue;
        } else if (err == WOLFSSL_ERROR_WANT_WRITE) {
            printf("[Handshake] Need to send data (WANT_WRITE)\n");
            continue;
        }
        fprintf(stderr, "[Handshake] ✗ FAILED with error: %d\n", err);
        char error_buf[80];
        wolfSSL_ERR_error_string(err, error_buf);
        fprintf(stderr, "[Handshake] Error string: %s\n", error_buf);
        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);
        close(net.sock);
        wolfSSL_Cleanup();
        return 1;
    }
    
    const char* cipher = wolfSSL_get_cipher(ssl);
    const char* version = wolfSSL_get_version(ssl);
    printf("\n=== Handshake Complete ===\n");
    printf("Protocol version: %s\n", version);
    printf("Cipher suite: %s\n", cipher);
    printf("Client certificate validated successfully!\n");
    printf("Ready to receive application data...\n\n");

    char buf[2048];
    printf("[Data] Waiting for application data from client...\n");
    ret = wolfSSL_read(ssl, buf, sizeof(buf));
    if (ret > 0) {
        printf("\n=== Application Data Received ===\n");
        printf("[Data] Received %d bytes of decrypted data\n", ret);
        printf("[Data] Content (as string): \"%.*s\"\n", ret, buf);
        printf("[Data] Content (hex): ");
        for (int i = 0; i < ret; i++) {
            printf("%02x ", (unsigned char)buf[i]);
            if ((i + 1) % 16 == 0) printf("\n                       ");
        }
        printf("\n");
        
        printf("[Data] Echoing back to client...\n");
        int write_ret = wolfSSL_write(ssl, buf, ret);
        if (write_ret == ret) {
            printf("[Data] ✓ Successfully echoed %d bytes back to client\n", write_ret);
        } else {
            int write_err = wolfSSL_get_error(ssl, write_ret);
            fprintf(stderr, "[Data] ✗ Echo failed: write returned %d, error: %d\n", write_ret, write_err);
        }
    } else {
        int err = wolfSSL_get_error(ssl, ret);
        fprintf(stderr, "[Data] ✗ wolfSSL_read failed: return=%d, error=%d\n", ret, err);
        char error_buf[80];
        wolfSSL_ERR_error_string(err, error_buf);
        fprintf(stderr, "[Data] Error string: %s\n", error_buf);
    }

    printf("\n=== Shutting Down ===\n");
    printf("[Cleanup] Freeing SSL session...\n");
    wolfSSL_free(ssl);
    printf("[Cleanup] Freeing SSL context...\n");
    wolfSSL_CTX_free(ctx);
    printf("[Cleanup] Closing socket...\n");
    close(net.sock);
    printf("[Cleanup] Cleaning up wolfSSL library...\n");
    wolfSSL_Cleanup();
    printf("[Cleanup] ✓ Server shutdown complete\n");
    return 0;
}
