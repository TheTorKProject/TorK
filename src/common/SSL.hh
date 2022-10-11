#ifndef SSL_HH
#define SSL_HH

SSL_CTX* init_client_ssl() {
    const SSL_METHOD *method;
    SSL_CTX* ctx;

    method = SSLv23_client_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    return ctx;
}

SSL_CTX* init_server_ssl() {
    const SSL_METHOD *method;
    SSL_CTX* ctx;

    method = SSLv23_server_method();  /* create new server-method instance */
    ctx = SSL_CTX_new(method);   /* create new context from method */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    return ctx;
}

void LoadSSLCertificate(SSL_CTX* ctx, const char* cert_path, const char* key_path) {
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
}

#endif /* SSL_HH */