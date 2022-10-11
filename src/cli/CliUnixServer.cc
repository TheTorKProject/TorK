#include "../common/Common.hh"

#include "CliUnixServer.hh"

const char* CliUnixServer::_socket_name;
int CliUnixServer::_type;
int CliUnixServer::_port;


CliUnixServer::CliUnixServer(const char* socket_name)
{
    _type = CLI_UNIX;
    _socket_name = socket_name;
    // setup variables
    _buflen = 1024;
    _buf = new char[_buflen+1];

    // setup handler for Control-C so we can properly unlink the UNIX
    // socket when that occurs
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = interrupt;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
}

CliUnixServer::CliUnixServer(const int port) {
    _type = CLI_TCP;
    _port = port;
    // setup variables
    _buflen = 1024;
    _buf = new char[_buflen+1];

    // setup handler for Control-C so we can properly unlink the UNIX
    // socket when that occurs
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = interrupt;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
}


CliUnixServer::~CliUnixServer() {
    delete[] _buf;
}


int CliUnixServer::initialize(Controller *controller, int run_mode)
{
    assert(controller != NULL);
    _controller = controller;

    if (run_mode == RUN_FOREGROUND) {
        create();
        serve();
        return 0;
    }
    else if (run_mode == RUN_BACKGROUND) {
        create();
        std::thread th(&CliUnixServer::serve, this);
        th.detach();
        return 0;
    }
    return -1;
}


void CliUnixServer::create()
{
    int reuseaddr = 1;
    struct sockaddr_un server_addr;
    struct sockaddr_in local;
    struct addrinfo hints, *res;

    if (_type == CLI_UNIX) {

        // setup socket address structure
        bzero(&server_addr,sizeof(server_addr));
        server_addr.sun_family = (_type == CLI_UNIX) ? AF_UNIX : AF_INET;
        strncpy(server_addr.sun_path, _socket_name, sizeof(server_addr.sun_path) - 1);

        // create socket
        _server = socket(PF_UNIX, SOCK_STREAM, 0);
                                        
    } else {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        const char *client_port = std::to_string(_port).c_str();
        if (getaddrinfo("127.0.0.1", client_port, &hints, &res) != 0) {
            perror("Fatal error: getaddrinfo");
            exit(EXIT_FAILURE);
        }
        _server = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    }

    if (_server < 0) {
        perror("Socket Operation");
        exit(EXIT_FAILURE);
    }

    if (_type == CLI_TCP && setsockopt(_server, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
        sizeof(int)) == -1)
    {
        perror("Fatal error: setsockopt");
        exit(EXIT_FAILURE);
    }

    if (_type == CLI_UNIX) {
        unlink(_socket_name);        
    }

    if (_type == CLI_UNIX) {
        // call bind to associate the socket with the UNIX file system
        if (bind(_server, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    } else {
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(_port);

        if (bind(_server, (struct sockaddr *)&local, sizeof(local)) == -1) {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }

      // convert the socket to listen for incoming connections
    if (listen(_server, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}


void CliUnixServer::close_socket()
{
    if (_type == CLI_UNIX) {
        unlink(_socket_name);
    }
    close(_server);
}


void CliUnixServer::serve()
{
    // setup client
    int client;
    struct sockaddr_un addr;
    socklen_t clientlen = sizeof(addr);
    std::set<int> cli_clients;
    std::queue<int> cli_clients_zombies;

    fd_set set, read_set;

    FD_ZERO(&set);
    FD_SET(_server, &set);

    while (true) {
        read_set = set;

        if (select (FD_SETSIZE, &read_set, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                break;
            } else {
                std::cerr << "Fatal CliUnixServer error: select() " << errno <<  std::endl;
                exit(EXIT_FAILURE);
            }
        }

        if (FD_ISSET(_server, &read_set)) {
            client = accept(_server,(struct sockaddr *)&addr, &clientlen);
            
            if (client > 0) {
                cli_clients.insert(client);
                FD_SET(client, &set);
            }
        }
        for (int cli : cli_clients) {
            if (FD_ISSET(cli, &read_set)) {
                if (!handle(cli)) {
                    FD_CLR(cli, &set);
                    cli_clients_zombies.push(cli);
                }
            }
        }
        while(!cli_clients_zombies.empty()) {
            int cli = cli_clients_zombies.front();
            cli_clients_zombies.pop();
            close(cli);
            cli_clients.erase(cli);
        }
    }

    close_socket();
}


bool CliUnixServer::handle(int client)
{
    std::string response;

    // get a request
    std::string request = get_request(client);
    // break if client is done or an error occurred

    if (_controller != NULL) {
        _controller->handleCliRequest(request, response);
    }

    // send response
    return send_response(client, response);   
    
}


std::string CliUnixServer::get_request(int client)
{
    std::string request = "";
    // read until we get a newline
    while (request.find("\n") == std::string::npos) {
        int nread = recv(client, _buf, 1024, 0);
        if (nread < 0) {
            if (errno == EINTR)
                // the socket call was interrupted -- try again
                continue;
            else
                // an error occurred, so break out
                return "";
        } else if (nread == 0) {
            // the socket is closed
            return "";
        }
        // be sure to use append in case we have binary data
        request.append(_buf, nread);
    }
    // a better server would cut off anything after the newline and
    // save it in a cache
    return request;
}


bool CliUnixServer::send_response(int client, std::string response)
{
    // prepare to send response
    const char* ptr = response.c_str();
    int nleft = response.length();
    int nwritten;
    // loop to be sure it is all sent
    
    while (nleft) {
        if ((nwritten = send(client, ptr, nleft, 0)) < 0) {
            if (errno == EINTR) {
                // the socket call was interrupted -- try again
                continue;
            } else {
                return false;
            }
        } else if (nwritten == 0) {
            return false;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }

    return true;
}


void CliUnixServer::interrupt(int)
{
    if (_type == CLI_UNIX)
        unlink(_socket_name);
}
