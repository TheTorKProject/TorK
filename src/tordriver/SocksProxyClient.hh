/**
 * SOCKS 4 and 5 proxy Implementation.
 *
 * Notes/Docs:
 * RFC 1928 "SOCKS Protocol Version 5" (https://tools.ietf.org/html/rfc1928)
 */
#ifndef SOCKSPROXYCLIENT_H
#define SOCKSPROXYCLIENT_H


#include <set>
#include <iterator>
#include "../common/Common.hh"

#include "../controller/ControllerClient.hh"


class SocksProxyClient {

    public:

        SocksProxyClient(){
            FD_ZERO (&_active_fd_set);
        };

        virtual ~SocksProxyClient(){};

        #if USE_SSL
            int initialize(ControllerClient *controller, bool direct_connect,
                           int fd_bridge, SSL_CTX *ssl_ctx, int run_mode);
        #else
            int initialize(ControllerClient *controller, bool direct_connect,
                           int fd_bridge, int run_mode);
        #endif

        int read_msg_client(FdPair *fd_pair, char *buff, int buffsize);

        int write_msg_client(FdPair *fd_pair, char *buff, int size);

        int readn_msg_bridge(FdPair *fd_pair, char *buff, int buffsize);

        int writen_msg_bridge(FdPair *fd_pair, char *buff, int size);

        int shutdown_connection(FdPair *fd_pair);

        int shutdown_local_connection(FdPair *fd_pair);

        void log(const char *message, ...);

        int init_conn_handler(int fd_bridge, int fd_client);

        void restore_conn_handler(int fd_bridge, int fd_client);

        int app_connect(int type, void *buf, unsigned short int portnum);

    private:

        void *main_thread();

        int socks_invitation(int fd, int *version);

        char* socks5_auth_get_user(int fd);

        char* socks5_auth_get_pass(int fd);

        int socks5_auth_userpass(int fd, char* arg_username, char* arg_password);

        int socks5_auth_noauth(int fd);

        void socks5_auth_notsupported(int fd);

        int socks5_auth(int fd, int methods_count, int auth_type,
                        char* arg_username, char* arg_password);

        int socks5_command(int fd);

        unsigned short int socks_read_port(int fd);

        char *socks_ip_read(int fd);

        void socks5_ip_send_response(int fd, char *ip, unsigned short int port);

        char *socks5_domain_read(int fd, unsigned char *size);

        void socks5_domain_send_response(int fd, char *domain, unsigned char size,
                                         unsigned short int port);

        int socks4_is_4a(char *ip);

        int socks4_read_nstring(int fd, char *buf, int size);

        void socks4_send_response(int fd, int status);

        int init_socks_session(int net_fd, int auth_type, char* arg_username,
                               char* arg_password, bool connect = true);

        int bind_socks_port();

    private:

        unsigned short int _port = 1080;

        ControllerClient *_controller;

        std::set<FdPair*> _fds;

        fd_set _active_fd_set;

        std::set<FdPair*> _zombies;

        #if USE_SSL
            SSL_CTX *_ssl_ctx;
        #endif

        /* Direct Connect to the bridge? */
        bool _direct_connect = false;

};

#endif //SOCKSPROXYCLIENT_H
