#ifndef CLIENT_MANAGER_HH
#define CLIENT_MANAGER_HH

#include <map>
#include <set>
#include <functional>
#include "Client.hh"
#include "FdPair.hh"
#include "FramePool.hh"

#define CLIENT_MANAGER_OK          (0)
#define CLIENT_MANAGER_ERR_INVALID (-1)

#define VALID_CLIENT(C) (C->getState() != CLIENT_STATE_NOT_CONN && \
                         C->getState() != CLIENT_STATE_UNDEF    && \
                         C->getState() != CLIENT_STATE_SHUT)

class ClientManager {

public:
    ClientManager();

    ~ClientManager();

    void add_client(FdPair *fdp);

    void remove_client(FdPair *fdp, FramePool *frame_pool);

    FrameQueue* getDataQueue(FdPair *fdp);

    FrameQueue* getCtrlQueue(FdPair *fdp);

    FrameQueue* getReceptionQueue(FdPair *fdp);

    Frame* getTmpFrame(FdPair *fdp);

    void setTmpFrame(FdPair *fdp, Frame *frame);

    int getTmpChunk(FdPair *fdp);

    void setTmpChunk(FdPair *fdp, int chunk);

    void safeIterate(std::function<void(FdPair*, Client*)> f);

    int updateClientState(FdPair *fdp, int new_state);

    int updateClientKMin(FdPair *fdp, int k_min);

    bool empty();

    int getNumberClients();

    int size();

    int getTotalDataFrames();

    int getTotalCtrlFrames();

    int getTotalRecpFrames();

    int getClientState(FdPair *fdp);

    int getClientKMin(FdPair *fdp);

    std::set<FdPair*> getBrokenClients();

    std::set<FdPair*> getFulfilledClients();

    std::set<FdPair*> getWaitingFulfilledClients();

    bool isClientBroken(FdPair *fdp);

    bool handleReceptionFrames(std::function<void(FdPair*, Client*)> f);

    void setReceptionMark(FdPair *fdp);

    Client* getClientInstance();

private:
    int connectedClients();

    int unallocFramesFromClient(Client* client, FramePool *frame_pool);

    std::map<FdPair*, Client*> _clients;

    std::shared_mutex _mtx;

};

#endif /* CLIENT_MANAGER_HH */