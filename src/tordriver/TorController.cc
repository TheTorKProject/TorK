#include "TorController.hh"
#include "../common/Common.hh"
#include "../common/RingBuffer.hh"

#include <unistd.h>
#include <errno.h>
#include <vector>
#include <string>
#include <thread>
#include <stdarg.h>
#include <assert.h>
#include <boost/algorithm/string/trim.hpp>


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static ssize_t readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    /* # of bytes fetched by last read() */
    size_t totRead;                     /* Total bytes read so far */
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = (char*) buffer;                /* No pointer arithmetic on "void *" */

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)         /* Interrupted --> restart read() */
                continue;
            else
                return -1;              /* Some other error */

        } else if (numRead == 0) {      /* EOF */
            if (totRead == 0)           /* No bytes read; return 0 */
                return 0;
            else                        /* Some bytes read; add '\0' */
                break;

        } else {                        /* 'numRead' must be 1 if we get here */
            if (totRead < n - 1) {
                totRead++;
                *buf++ = ch;
            } else {
                break;
            }

            if (ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
}


static void tokenize(std::string const &str, const char delim,
    std::vector<std::string> &out)
{
	size_t start;
	size_t end = 0;

	while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
	{
		end = str.find(delim, start);
		out.push_back(str.substr(start, end - start));
	}
}


void TorController::log(const char *message, ...)
{
    char vbuffer[255];
    va_list args;
    va_start(args, message);
    vsnprintf(vbuffer, ARRAY_SIZE(vbuffer), message, args);
    va_end(args);

    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';

    pthread_t self = pthread_self();

    fprintf(stderr, "[TORK] [%lu]: %s\"\n", self, vbuffer);
    fflush(stderr);
}


void TorController::fatal(const char *message, ...)
{
    char vbuffer[255];
    va_list args;
    va_start(args, message);
    vsnprintf(vbuffer, ARRAY_SIZE(vbuffer), message, args);
    va_end(args);

    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';

    pthread_t self = pthread_self();

    fprintf(stderr, "[TORK] [%lu]: FATAL error: %s\"\n", self, vbuffer);
    fflush(stderr);

    exit(1);
}


void TorController::beginCmd()
{
    std::unique_lock<std::mutex> res_lock(_cmd_mtx);
    while (_cmd_in_exec) {
        _cmd.wait(res_lock);
    }
    _cmd_in_exec = true;
}


void TorController::endCmd()
{
    assert(_cmd_in_exec == true);
    std::unique_lock<std::mutex> res_lock(_cmd_mtx);
    _cmd_in_exec = false;
    _cmd.notify_all();
}


int TorController::cmdExtendCircuit(int &circuitID)
{
    char buffer[1000];
    int status = 0;
    std::string server_spec = "";

    beginCmd();
    if (server_spec == "") {
        sprintf(buffer, "EXTENDCIRCUIT 0 purpose=general\r\n");
    } else {
        sprintf(buffer, "EXTENDCIRCUIT %s purpose=general\r\n", server_spec.c_str());
    }
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Extend circuit: protocol error [1].");
    }

    std::string reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        endCmd();
        fatal("Extend circuit: protocol error [2].");
    }

    std::vector<std::string> parsed_reply;
    tokenize(reply, ' ', parsed_reply);

    if (parsed_reply[0] == "250" && parsed_reply[1] == "EXTENDED") {
        circuitID = std::stoi(parsed_reply[2]);
        endCmd();

        return TORCTL_CMD_OK;

    } else if (parsed_reply[0] == "552") {
        circuitID = -1; //parsed_reply[4];
        endCmd();

        return TORCTL_CIRCUIT_ERROR_NO_ROUTER;

    } else if (parsed_reply[0] == "551") {
        circuitID = -1;
        endCmd();

        return TORCTL_CIRCUIT_ERROR_CANNOT_START;
    }
    else {
        circuitID = -1;
        endCmd();

        return TORCTL_CMD_ERROR;
    }

}


int TorController::cmdAttachStream(int stream, int circ)
{
    char buffer[1000];
    int status = 0;

    beginCmd();
    sprintf(buffer, "ATTACHSTREAM %d %d\r\n", stream, circ);
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Attach stream: protocol error [1].");
    }

    std::string reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        endCmd();
        fatal("Attach stream: protocol error [2].");
    }

    if (reply.find("250 OK") != std::string::npos) {
        endCmd();
        return TORCTL_CMD_OK;
    }

    if (reply.find("552") != std::string::npos) {
        endCmd();
        return TORCTL_ATTACH_ERROR_DONT_EXIST;
    }
    endCmd();

    return TORCTL_CMD_ERROR;
}


int TorController::cmdSetEventsCirc()
{
    char buffer[100];
    int status;

    beginCmd();
    sprintf(buffer, "SETEVENTS CIRC\r\n");
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Set events circ: protocol error [1].");
    }

    std::string reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        endCmd();
        fatal("Set events circ: protocol error [2].");
    }

    if (reply.find("250 OK") == std::string::npos) {
        log("Set events circ: protocol error [3].");
        endCmd();
        return TORCTL_CMD_ERROR;
    }
    endCmd();

    return TORCTL_CMD_OK;
}


int TorController::cmdSetEventsStream()
{
    char buffer[100];
    int status;

    beginCmd();
    sprintf(buffer, "SETEVENTS STREAM\r\n");
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Set events stream: protocol error [1].");
    }

    std::string reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        endCmd();
        fatal("Set events stream: protocol error [2].");
    }

    if (reply.find("250 OK") == std::string::npos) {
        log("Set events stream: protocol error [3].\n");
        endCmd();
        return TORCTL_CMD_ERROR;
    }
    endCmd();

    return TORCTL_CMD_OK;
}


int TorController::cmdGetInfoVersion(std::string &version)
{
    char buffer[100];
    int status;

    beginCmd();
    sprintf(buffer, "GETINFO version\r\n");
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Get info version: protocol error [1].");
    }

    std::string reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        endCmd();
        fatal("Get info version: protocol error [2].");
    }

    std::vector<std::string> parsed_reply;
    tokenize(reply, '=', parsed_reply);

    if (parsed_reply[0].find("250-version") == std::string::npos) {
        log("Get info version: protocol error [3].\n");
        endCmd();
        return TORCTL_CMD_ERROR;
    }

    version = parsed_reply[1];

    reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        log("Get info version: protocol error [4].\n");
        endCmd();
        return TORCTL_CMD_ERROR;
    }
    if (reply.find("250 OK") == std::string::npos) {
        log("Get info version: protocol error [5].\n");
        endCmd();
        return TORCTL_CMD_ERROR;
    }
    endCmd();

    return TORCTL_CMD_OK;
}


int TorController::cmdGetInfoStreamStatus(std::vector<std::string> &streams)
{
    char buffer[100];
    int status;

    beginCmd();
    sprintf(buffer, "GETINFO stream-status\r\n");
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Get info stream status: protocol error [1].");
    }

    while (true) {
        std::string reply = _rb_cmd->get(&status);
        if (status != RINGBUFFER_STATUS_OK) {
            endCmd();
            fatal("Get info stream status: protocol error [2].");
        }

        if (reply.find("250 OK") != std::string::npos) {
            endCmd();
            return TORCTL_CMD_OK;
        } else {
            streams.push_back(reply);
        }
    }
    endCmd();

    return TORCTL_CMD_OK;
}

int TorController::cmdCloseCircuit(int &circuitID) {

    char buffer[100];
    int status;

    beginCmd();
    sprintf(buffer, "CLOSECIRCUIT %d\r\n", circuitID);
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Get info stream status: protocol error [1].");
    }

    std::string reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        endCmd();
        fatal("Attach stream: protocol error [2].");
    }

    if (reply.find("250 OK") != std::string::npos) {
        endCmd();
        return TORCTL_CMD_OK;
    }

    if (reply.find("552") != std::string::npos) {
        endCmd();
        return TORCTL_CIRCUIT_ERROR_DONT_EXIST;
    }

    if (reply.find("512") != std::string::npos) {
        endCmd();
        return TORCTL_CIRCUIT_ERROR_ARGS;
    }
    endCmd();

    return TORCTL_CMD_ERROR;
}

int TorController::cmdSendSignal(int signal) {
    char buffer[100];
    int status;
    std::string reply, signal_str;

    if (TCTL_INVALID_SIGNAL(signal)) {
        return TCTL_SIGNAL_ERROR_INVALID;
    }

    switch (signal) {
        case TCTL_SIGNAL_RELOAD  : signal_str = "RELOAD"; break;
        case TCTL_SIGNAL_SHUTDOWN: signal_str = "SHUTDOWN"; break;
        case TCTL_SIGNAL_DORMANT : signal_str = "DORMANT"; break;
        case TCTL_SIGNAL_ACTIVE  : signal_str = "ACTIVE"; break;
        case TCTL_SIGNAL_NEWNYM  : signal_str = "NEWNYM";
    }

    beginCmd();
    sprintf(buffer, "SIGNAL %s\r\n", signal_str.c_str());
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        endCmd();
        fatal("Send Signal: protocol error [1].");
        return TORCTL_CMD_ERROR;
    }

    reply = _rb_cmd->get(&status);
    if (status != RINGBUFFER_STATUS_OK) {
        log("Send Signal: protocol error [2].\n");
        endCmd();
        return TORCTL_CMD_ERROR;
    }
    if (reply.find("250 OK") == std::string::npos) {
        log("Send Signal: protocol error [3].\n");
        endCmd();
        return TORCTL_CMD_ERROR;
    }
    endCmd();

    return TORCTL_CMD_OK;
}

void *TorController::threadEvent()
{
    int status = -1;
    std::string msg;

    while (true) {

        msg = _rb_evt->get(&status);
        if (status != RINGBUFFER_STATUS_OK) {
            fatal("Event handler thread: protocol error [1].\n");
        }

        std::vector<std::string> parsed_event;
        tokenize(msg, ' ', parsed_event);

        TorEvent event;
        event._type = TCTL_EVENT_OTHER;
        event._descr = msg;

        if (parsed_event.size() > 3 && parsed_event[1] == "CIRC" &&
            parsed_event[3] == "BUILT") {

            event._type = TCTL_EVENT_CIRC_BUILT;
            event._circ = std::stoi(parsed_event[2]);
        }
        if (parsed_event.size() > 3 && parsed_event[1] == "CIRC" &&
            parsed_event[3] == "CLOSED") {

            event._type = TCTL_EVENT_CIRC_CLOSED;
            event._circ = std::stoi(parsed_event[2]);
        }
        if (parsed_event.size() > 3 && parsed_event[1] == "CIRC" &&
            parsed_event[3] == "FAILED") {

            event._type = TCTL_EVENT_CIRC_FAILED;
            event._circ = std::stoi(parsed_event[2]);
        }
        if (parsed_event.size() > 3 && parsed_event[1] == "STREAM" &&
            parsed_event[3] == "NEW") {

            event._type = TCTL_EVENT_STREAM_NEW;
            event._stream = std::stoi(parsed_event[2]);
        }
        if (parsed_event.size() > 3 && parsed_event[1] == "STREAM" &&
            parsed_event[3] == "CLOSED") {

            event._type = TCTL_EVENT_STREAM_CLOSED;
            event._stream = std::stoi(parsed_event[2]);
        }
        if (parsed_event.size() > 3 && parsed_event[1] == "STREAM" &&
            parsed_event[3] == "FAILED") {

            event._type = TCTL_EVENT_STREAM_FAILED;
            event._stream = std::stoi(parsed_event[2]);
        }
        if (parsed_event.size() == 4 && parsed_event[1] == "STATUS_CLIENT" &&
            parsed_event[2] == "NOTICE" &&
            parsed_event[3] == "ENOUGH_DIR_INFO\r\n") {

            event._type = TCTL_EVENT_STC_ENOUGH_DIR_INFO;
        }

        _controller_client->handleTorCtlEventReceived(&event);
    }

    return NULL;
}


void *TorController::threadMain()
{
    struct sockaddr_in serv_addr;
    char buffer[1000];
    char reply[1000];

#if (LOG_VERBOSE & LOG_CTRL_EVENTS)
    log("Tor controller connecting...");
#endif

    _control_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_control_fd < 0) {
        fatal("Socket creation failure!");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_control_port);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        fatal("Invalid address.");
    }
    if (connect(_control_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fatal("Connection failure.");
    }

#if (LOG_VERBOSE & LOG_CTRL_EVENTS)
    log("Tor controller authenticating...");
#endif

    sprintf(buffer, "AUTHENTICATE \"%s\"\r\n", "password");
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        fatal("Authentication send operation failure.");
    }

    if (recv(_control_fd, reply, sizeof(reply), 0) <= 0) {
        fatal("Authentication receive failure.");
    } else if (strstr(reply, "OK") == NULL) {
        fatal("Authentication error: %s", reply);
    }

#if (LOG_VERBOSE & LOG_CTRL_EVENTS)
    log("Tor controller sign up for events...");
#endif

    sprintf(buffer, "SETEVENTS CIRC STREAM STATUS_CLIENT\r\n");
    if (send(_control_fd, buffer, strlen(buffer), 0) <= 0) {
        fatal("Set events circ error.");
    }

    if (recv(_control_fd, reply, sizeof(reply), 0) <= 0) {
        fatal("Set events circ receive error.");
    } else if (strstr(reply, "250 OK") == NULL) {
        fatal("Set events circ protocol error.");
    }

    _controller_client->handleTorCtlInitialized();

    int status;
    std::string msg;

    while (true) {
        msg.clear();
        do {
            if (readLine(_control_fd, buffer, sizeof(buffer)) <= 0) {
                fatal("Tor controller connection failure.");
            }
            msg += buffer;
        } while(strstr(buffer, "\n") == NULL);

        #if (LOG_VERBOSE & LOG_CTRL_EVENTS)
            log("FROM TOR: %s.\n", msg.c_str());
        #endif

        if (msg.substr(0, 3).find("650") != std::string::npos) {
            _rb_evt->put(msg, &status);
            if (status != RINGBUFFER_STATUS_OK) {
                fatal("Failed to dispatch event to handler thread.");
            }
        } else {
            _rb_cmd->put(msg, &status);
            if (status != RINGBUFFER_STATUS_OK) {
                fatal("Failed to dispatch reply to command thread.");
            }
        }
    }

    return NULL;
}


int TorController::initialize(ControllerClient *controller, int run_mode)
{
    assert(controller != NULL);
    _controller_client = controller;

    _control_port = _controller_client->get_torctl_port();

    _rb_cmd->enable();
    _rb_evt->enable();

    std::thread th_evt(&TorController::threadEvent, this);
    th_evt.detach();

    if (run_mode == RUN_BACKGROUND) {
        std::thread th_main(&TorController::threadMain, this);
        th_main.detach();
        return 0;
    }

    if (run_mode == RUN_FOREGROUND) {
        threadMain();
        return 0;
    }

    return -1;
}

