#ifndef TORCONTROLLER_H
#define TORCONTROLLER_H


#include "../common/Common.hh"
#include "../common/RingBuffer.hh"
#include "../controller/ControllerClient.hh"


#define TORCTL_CMD_OK                       (0)
#define TORCTL_CMD_ERROR                    (-1)
#define TORCTL_CIRCUIT_ERROR_NO_ROUTER      (-2)
#define TORCTL_ATTACH_ERROR_DONT_EXIST      (-3)
#define TORCTL_CIRCUIT_ERROR_DONT_EXIST     (-4)
#define TORCTL_CIRCUIT_ERROR_ARGS           (-5)
#define TORCTL_CIRCUIT_ERROR_CANNOT_START   (-6)

#define TCTL_EVENT_CIRC_BUILT               (1)
#define TCTL_EVENT_CIRC_CLOSED              (2)
#define TCTL_EVENT_CIRC_FAILED              (3)
#define TCTL_EVENT_STREAM_NEW               (4)
#define TCTL_EVENT_STREAM_CLOSED            (5)
#define TCTL_EVENT_STREAM_FAILED            (6)
#define TCTL_EVENT_STC_ENOUGH_DIR_INFO      (7)
#define TCTL_EVENT_OTHER                    (99)

#define TCTL_SIGNAL_RELOAD                  (1)
#define TCTL_SIGNAL_SHUTDOWN                (2)
#define TCTL_SIGNAL_DORMANT                 (3)
#define TCTL_SIGNAL_ACTIVE                  (4)
#define TCTL_SIGNAL_NEWNYM                  (5)
#define TCTL_SIGNAL_ERROR_INVALID           (-1)

#define TCTL_INVALID_SIGNAL(s) (s != TCTL_SIGNAL_RELOAD      \
                                && s != TCTL_SIGNAL_SHUTDOWN \
                                && s != TCTL_SIGNAL_DORMANT  \
                                && s != TCTL_SIGNAL_ACTIVE)

struct TorEvent {
    int _type;
    int _stream;
    int _circ;
    std::string _descr;
};

class TorController {

    public:

        TorController() {
            _rb_cmd = new RingBuffer<std::string>(10);
            _rb_evt = new RingBuffer<std::string>(10);
        };

        virtual ~TorController() {
            close(_control_fd);
        };

        int initialize(ControllerClient *controller, int run_mode);

        int cmdGetInfoVersion(std::string &version);

        int cmdGetInfoStreamStatus(std::vector<std::string> &streams);

        int cmdSetEventsCirc();

        int cmdSetEventsStream();

        int cmdExtendCircuit(int &circuitID);

        int cmdAttachStream(int stream, int circ);

        int cmdCloseCircuit(int &circuitID);

        int cmdSendSignal(int signal);

        static void log(const char *message, ...);

        static void fatal(const char *message, ...);

    private:

        void *threadMain();

        void *threadEvent();

        void beginCmd();

        void endCmd();

    private:

        int _control_port;

        int _control_fd;

        bool _events_on = false;

        void (*_event_handler)(TorController *controller, std::string reply);

        RingBuffer<std::string>* _rb_cmd;

        ControllerClient *_controller_client;

        RingBuffer<std::string>* _rb_evt;

        std::mutex _cmd_mtx;

        std::condition_variable _cmd;

        bool _cmd_in_exec = false;
};

#endif /* TORCONTROLLER_H */
