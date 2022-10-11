#include "CliUnixClient.hh"

CliUnixClient::CliUnixClient(const char* socket_name) {
    // setup variables
    _buflen = 1024;
    _buf = new char[_buflen+1];
    _socket_name = socket_name;
}

CliUnixClient::~CliUnixClient() {
    delete[] _buf;
}

void CliUnixClient::run() {
    // connect to the server and run echo program
    create();
    echo();
}

void CliUnixClient::create() {
    struct sockaddr_un server_addr;

    // setup socket address structure
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, _socket_name, sizeof(server_addr.sun_path) - 1);

    // create socket
    _server = socket(PF_UNIX,SOCK_STREAM,0);
    if (!_server) {
        perror("socket");
        exit(-1);
    }

    // connect to server
    if (connect(_server, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(-1);
    }
}

void CliUnixClient::close_socket() {
    close(_server);
}

void CliUnixClient::echo() {
    std::string line;
    
    // loop to handle user interface
    while (getline(std::cin, line)) {
        // append a newline
        line += "\n";
        // send request
        bool success = send_request(line);
        // break if an error occurred
        if (!success)
            break;
        // get a response
        success = get_response();
        // break if an error occurred
        if (!success)
            break;
    }
    close_socket();
}

bool CliUnixClient::send_request(std::string request) {
    // prepare to send request
    const char* ptr = request.c_str();
    int nleft = request.length();
    int nwritten;
    // loop to be sure it is all sent
    while (nleft) {
        if ((nwritten = send(_server, ptr, nleft, 0)) < 0) {
            if (errno == EINTR) {
                // the socket call was interrupted -- try again
                continue;
            } else {
                return false;
            }
        } else if (nwritten == 0) {
            // the socket is closed
            return false;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return true;
}

bool CliUnixClient::get_response() {
    std::string response = "";
    // read until we get a newline
    while (response.find("\n") == std::string::npos) {
        int nread = recv(_server, _buf, 1024, 0);
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
        response.append(_buf, nread);
    }
    // a better client would cut off anything after the newline and
    // save it in a cache
    std::cout << response;
    return true;
}
