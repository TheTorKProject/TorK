#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>

#include <fstream>
#include <iostream>
#include <string>

class CliUnixClient {

public:
    CliUnixClient(const char* socket_name);

    ~CliUnixClient();

    void run();

protected:
    virtual void create();

    virtual void close_socket();

    void echo();

    bool send_request(std::string);

    bool get_response();

private:
    int _server;

    int _buflen;

    char* _buf;

    const char* _socket_name;
};
