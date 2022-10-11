#ifndef SOCKSPROXYSERVER_H
#define SOCKSPROXYSERVER_H

#include <set>
#include "../common/Common.hh"

#include "../controller/ControllerServer.hh"


class SocksProxyServer {

    public:

        SocksProxyServer(){};

        virtual ~SocksProxyServer(){};

        #if USE_SSL
            int initialize(ControllerServer *controller, SSL_CTX *ssl_ctx,
                           int port_cli, int port_local, int run_mode);
        #else
            int initialize(ControllerServer *controller, int port_cli,
                           int port_local, int run_mode);
        #endif

        int readn_msg_client(FdPair *fd_pair, char *buff, int buffsize);

        int writen_msg_client(FdPair *fd_pair, char *buff, int size);

        int read_msg_local(FdPair *fd_pair, char *buff, int buffsize);

        int write_msg_local(FdPair *fd_pair, char *buff, int size);

        int writen_msg_local(FdPair *fd_pair, char *buff, int size);

        int shutdown_connection(FdPair *fd_pair);

        int shutdown_local_connection(FdPair *fd_pair);

        int restore_local_connection(FdPair *fd_pair);

        void log(const char *message, ...);

    private:

        void *main_thread();

    private:

        int _port_cli;

        int _port_local;

        ControllerServer *_controller;

        std::set<FdPair*> _fds;

        fd_set _active_fd_set;

        std::set<FdPair*> _zombies;

        #if USE_SSL
            SSL_CTX *_ssl_ctx;
        #endif

};

#endif //SOCKSPROXYSERVER_H
