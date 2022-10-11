#ifndef CONTROLLERSERVER_HH
#define CONTROLLERSERVER_HH

#include "TrafficShaper.hh"
#include "FramePool.hh"
#include "ClientManager.hh"
#include <map>

class TorPTServer;
class SocksProxyServer;
class CliUnixServer;
class FdPair;

class ControllerServer : public Controller {

public:

    ControllerServer(int max_chunks, int chunk_size, int ts_min_rate,
                     int ts_max_rate, TorPTServer *pt, SocksProxyServer *sp,
                     CliUnixServer *cli, TrafficShaper *ts);

    ~ControllerServer(){};

    void handleSocksNewConnection(FdPair *fds);

    void handleSocksClientDataReady(FdPair *fds);

    void handleSocksBridgeDataReady(FdPair *fds);

    void handleSocksConnectionTerminated(FdPair *fds);

    void handleCliRequest(std::string &request, std::string &response);

    void handleTrafficShapingEvent();

    void handleCtrlFrame(FdPair *fdp, Frame *frame);

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

protected:

    SocksProxyServer *_sp;

    TorPTServer *_pt;

    CliUnixServer *_cli;

    int _max_chunks;
    int _chunk_size;
    int _ts_min_rate;
    int _ts_max_rate;

    FramePool _frame_pool;
    Frame _chaff_frame;

    TrafficShaper *_ts;

    ClientManager _client_manager;

private:
    void order_CHANGE();
    void order_WAIT();
    void order_TS_RATE(unsigned int rate);
    void ts_rate_update();

    #if STATS
        Stats _stats;
    #endif

    #if TIME_STATS
        TimeStats _time_stats;
    #endif

    #if SYNC_DLV_STATS
        SyncDLVStats _dlv_stats;
    #endif

};


#endif //CONTROLLERSERVER_HH
