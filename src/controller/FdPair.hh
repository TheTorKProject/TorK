#ifndef FDPAIR_HH
#define FDPAIR_HH

#include "../common/Common.hh"

#define INV_FD (-1)

class FdPair {

    public:

        #if USE_SSL
            FdPair(int fd0, int fd1, SSL *ssl): _fd0(fd0), _fd1(fd1), _ssl(ssl) {};
        #else
            FdPair(int fd0, int fd1): _fd0(fd0), _fd1(fd1) {};
        #endif

        ~FdPair() {}

        int get_fd0() {
            return _fd0;
        }

        int get_fd1() {
            return _fd1;
        }

        void set_fd0(int fd0) {
            _fd0 = fd0;
        }

        void set_fd1(int fd1) {
            _fd1 = fd1;
        }

        #if USE_SSL
            void setSSL(SSL* ssl) {
                _ssl = ssl;
            }

            SSL* getSSL() {
                return _ssl;
            }

            int SSL_readn(void *buf, int n)
            {
                int nread, error;
                std::unique_lock<std::mutex> res_lock(_ssl_mtx);
                nread = SSL_peek(_ssl, buf, n);
                error = SSL_get_error(_ssl, nread);
                switch (nread) {
                    case 0 : return 0;
                    case -1: return ((error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) ? SSL_TRY_LATER : -1);
                    default: return ((nread == n) ? SSL_read(_ssl, buf, n) : SSL_TRY_LATER);
                }
            }

            int SSL_writen(void *buf, int n)
            {
                int nwrite, error;
                std::unique_lock<std::mutex> res_lock(_ssl_mtx);
                nwrite = SSL_write(_ssl, buf, n);
                error = SSL_get_error(_ssl, nwrite);
                switch (nwrite) {
                    case 0 : return 0;
                    case -1: return ((error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) ? SSL_TRY_LATER : -1);
                    default: return nwrite;
                }
            }
        #endif

    private:

        int _fd0;
        int _fd1;

        #if USE_SSL
            SSL* _ssl;
            std::mutex _ssl_mtx;
        #endif
};

#endif //FDPAIR_HH
