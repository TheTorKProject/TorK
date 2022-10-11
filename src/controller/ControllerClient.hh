#ifndef CONTROLLERCLIENT_HH
#define CONTROLLERCLIENT_HH

#include "TrafficShaper.hh"
#include "FramePool.hh"
#include "ClientManager.hh"

class TorPTClient;
class SocksProxyClient;
class TorController;
class CliUnixServer;

struct TorEvent;

#define NO_CIRCUIT       (-1)
#define CHANGE_CIRCUIT   (-2)
#define WAITING_DIR_INFO (-3)
#define WAITING_RESTORE  (-4)

#define CIRC_STATE_UNDEF      (0)
#define CIRC_STATE_BUILT      (1)

class ControllerClient : public Controller {

public:

    ControllerClient(int max_chunks, int chunk_size, int ts_min_rate,
                     int ts_max_rate, int k_min, bool ch_active, bool abort_on_conn,
                     TorPTClient *pt, SocksProxyClient *sp, TorController *tc,
                     CliUnixServer *cli, TrafficShaper *ts);

    ~ControllerClient(){};

    int get_socks_port() {
        return _socks_port;
    };

    int get_torctl_port() {
        return _torctl_port;
    };

    void config(int socks_port, int torctl_port);

    void handleCliRequest(std::string &request, std::string &response);

    void handleSocksNewConnection(FdPair *fds);

    void handleSocksRestoreConnection(FdPair *fds);

    void handleSocksClientDataReady(FdPair *fds);

    void handleSocksBridgeDataReady(FdPair *fds);

    void handleSocksConnectionTerminated(FdPair *fds);

    void handleSocksNewSessionError();

    void handleTorCtlInitialized();

    void handleTorCtlEventReceived(TorEvent *event);

    void handleTrafficShapingEvent();

    void handleCtrlFrame_NULL     (FdPair *fdp);
    void handleCtrlFrame_HELLO    (FdPair *fdp, FrameControlFields &fcf);
    void handleCtrlFrame_HELLO_OK (FdPair *fdp);
    void handleCtrlFrame_ACTIVE   (FdPair *fdp);
    void handleCtrlFrame_WAIT     (FdPair *fdp);
    void handleCtrlFrame_CHANGE   (FdPair *fdp);
    void handleCtrlFrame_CHANGE_OK(FdPair *fdp);
    void handleCtrlFrame_INACTIVE (FdPair *fdp);
    void handleCtrlFrame_SHUT     (FdPair *fdp);
    void handleCtrlFrame_SHUT_OK  (FdPair *fdp);
    void handleCtrlFrame_TS_RATE  (FdPair *fdp, FrameControlFields &fcf);

    void handleCtrlFrame_ERR_HELLO   (FdPair *fdp);
    void handleCtrlFrame_ERR_ACTIVE  (FdPair *fdp);
    void handleCtrlFrame_ERR_INACTIVE(FdPair *fdp);

    void handleCtrlFrame_UNKNOWN     (FdPair *fdp);

    void announce_k();
    void set_channel_status(bool enabled);

protected:

    bool create_circuit();

    int shutdown_local_helper(FdPair *fdp, int circ_val);

    int _socks_port = -1;

    int _torctl_port = -1;

    SocksProxyClient *_sp;

    TorController *_tc;

    TorPTClient *_pt;

    CliUnixServer *_cli;

    int _circ = NO_CIRCUIT;
    int _circ_state = CIRC_STATE_UNDEF;

    int _max_chunks;
    int _chunk_size;
    int _ts_min_rate;
    int _ts_max_rate;
    int _k_min;
    bool _ch_active_startup;
    bool _abort_on_conn;

    FramePool _frame_pool;
    Frame _chaff_frame;

    TrafficShaper *_ts;

    ClientManager _client_manager;

    std::set<int> _pending_streams;

    /* N attempts to create a circuit */
    int _circ_attempts = 0;

    /* Does Tor already have circuit consensus so we can create circuits? */
    bool _dir_info = false;

    #if STATS
        Stats _stats;
    #endif

    #if DEBUG_TOOLS
        DebugInfo _debug_info;
    #endif
};


#endif //CONTROLLERCLIENT_HH
