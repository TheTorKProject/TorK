#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <set>
#include <queue>
#include <thread>

#include "../controller/Controller.hh"

#define INV_PORT    (-1)
#define CLI_UNIX    (0)
#define CLI_TCP     (1)

class CliUnixServer {

    public:
    
        CliUnixServer(const char* socket_name);

        CliUnixServer(const int port);
    
        ~CliUnixServer();

        int initialize(Controller *controller, int run_mode);
        
    protected:
    
        virtual void create();
    
        virtual void close_socket();
    
        void serve();
    
        bool handle(int);
    
        std::string get_request(int);
    
        bool send_response(int, std::string);

        int _server;
    
        int _buflen;
    
        char* _buf;

    private:

        Controller *_controller;

        static void interrupt(int);
        
        static const char* _socket_name;

        static int _type;

        static int _port;
};
