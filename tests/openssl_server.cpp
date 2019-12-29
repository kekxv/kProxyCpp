#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <string>

#include <sys/socket.h>
#include <netinet/in.h>

#define IP_ADDR INADDR_ANY
#define PORT 8081

int password_cb(char *buf, int size, int rwflag, void *password);

EVP_PKEY *generatePrivateKey();
X509 *generateCertificate(EVP_PKEY *pkey);

/**
 * Example SSL server that accepts a client and echos back anything it receives.
 * Test using `curl -I https://127.0.0.1:8081 --insecure`
 */
int main(int arc, char **argv)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("opening socket\n");
        return -4;
    }

    struct sockaddr_in s_addr;
    bzero((char *)&s_addr, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = IP_ADDR;
    s_addr.sin_port = htons(PORT);

    if (bind(fd, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) {
        printf("binding\n");
        return -5;
    }

    listen(fd, 2);

    int cfd;
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    OpenSSL_add_all_algorithms();
    SSL_library_init();

    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    if (ctx == NULL) {
        printf("errored; unable to load context.\n");
        ERR_print_errors_fp(stderr);
        return -3;
    }

//        EVP_PKEY *pkey = generatePrivateKey();
//        X509 *x509 = generateCertificate(pkey);

//设置服务器证书
    if(SSL_CTX_use_certificate_file(ctx, "../tests/localhost.crt", SSL_FILETYPE_PEM)<=0)
    {
        ERR_print_errors_fp(stderr);
        exit(3);
    }
//设置服务器私钥
    if(SSL_CTX_use_PrivateKey_file(ctx, "../tests/localhost.key", SSL_FILETYPE_PEM)<=0)
    {
        ERR_print_errors_fp(stderr);
        exit(4);
    }

//检查私钥和证书是否匹配
    if(!SSL_CTX_check_private_key(ctx))
    {
        fprintf(stderr,"private key does not match the certificate public key\n");
        exit(5);
    }

//        SSL_CTX_use_certificate(ctx, x509);
    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
//        SSL_CTX_use_PrivateKey(ctx, pkey);

    RSA *rsa=RSA_generate_key(512, RSA_F4, NULL, NULL);
    SSL_CTX_set_tmp_rsa(ctx, rsa);
    RSA_free(rsa);

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);

    while (cfd = accept(fd, 0, 0))
    {

        SSL *ssl = SSL_new(ctx);
        BIO *accept_bio = BIO_new_socket(cfd, BIO_CLOSE);
        SSL_set_bio(ssl, accept_bio, accept_bio);

        SSL_accept(ssl);

        ERR_print_errors_fp(stderr);

        BIO *bio = BIO_pop(accept_bio);

        char buf[1024];
        while (1)
        {
            // first read data
            int r = SSL_read(ssl, buf, 1024);
            switch (SSL_get_error(ssl, r))
            {
                case SSL_ERROR_NONE:
                    break;
                case SSL_ERROR_ZERO_RETURN:
                    goto end;
                default:
                    printf("SSL read problem");
                    goto end;
            }

            int len = r;

            printf("%s\n", buf);

            // now keep writing until we've written everything
            int offset = 0;
            while (len)
            {
                r = SSL_write(ssl, buf + offset, len);
                switch (SSL_get_error(ssl, r))
                {
                    case SSL_ERROR_NONE:
                        len -= r;
                        offset += r;
                        break;
                    default:
                        printf("SSL write problem");
                        goto end;
                }
            }
        }

        end:

        SSL_shutdown(ssl);

        BIO_free_all(bio);
        BIO_free_all(accept_bio);
    }

    return 0;
}

int password_cb(char *buf, int size, int rwflag, void *password)
{
    strncpy(buf, (char *)(password), size);
    buf[size - 1] = '\0';
    return strlen(buf);
}

EVP_PKEY *generatePrivateKey()
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    EVP_PKEY_keygen_init(pctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048);
    EVP_PKEY_keygen(pctx, &pkey);
    return pkey;
}

X509 *generateCertificate(EVP_PKEY *pkey)
{
    X509 *x509 = X509_new();
    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 0);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), (long)60*60*24*365);
    X509_set_pubkey(x509, pkey);

    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (const unsigned char*)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"YourCN", -1, -1, 0);
    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_md5());
    return x509;
}