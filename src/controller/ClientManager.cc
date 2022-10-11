#include "ClientManager.hh"

ClientManager::ClientManager() {

}

ClientManager::~ClientManager() {
    std::unique_lock<std::shared_mutex> res_lock(_mtx);
    for (std::pair<FdPair*, Client*> x : _clients) {
        delete _clients[x.first];
    }
}

void ClientManager::add_client(FdPair *fdp) {
    std::unique_lock<std::shared_mutex> res_lock(_mtx);
    _clients.insert(std::make_pair(fdp, new Client()));
}

void ClientManager::remove_client(FdPair *fdp, FramePool *frame_pool) {
    int status;
    {
        std::unique_lock<std::shared_mutex> res_lock(_mtx);
        assert(_clients.find(fdp) != _clients.end());
        assert(frame_pool != nullptr);
        status = unallocFramesFromClient(_clients[fdp], frame_pool);
        assert(status == FRAME_POOL_OK);

        delete _clients[fdp];
        _clients.erase(fdp);
    }
}

FrameQueue* ClientManager::getDataQueue(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getDataQueue();
}

FrameQueue* ClientManager::getCtrlQueue(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getCtrlQueue();
}

FrameQueue* ClientManager::getReceptionQueue(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getReceptionQueue();
}

Frame* ClientManager::getTmpFrame(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getTmpFrame();
}

void ClientManager::setTmpFrame(FdPair *fdp, Frame *frame) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    _clients[fdp]->setTmpFrame(frame);
}

int ClientManager::getTmpChunk(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getTmpChunk();
}

void ClientManager::setTmpChunk(FdPair *fdp, int chunk) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    _clients[fdp]->setTmpChunk(chunk);
}

void ClientManager::safeIterate(std::function<void(FdPair*, Client*)> f) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    for (std::pair<FdPair*, Client*> x : _clients) {
        f(x.first, x.second);
    }
}

int ClientManager::updateClientState(FdPair *fdp, int new_state) {
    if (new_state != CLIENT_STATE_CONNECTED && new_state != CLIENT_STATE_ACTIVE &&
        new_state != CLIENT_STATE_INACTIVE && new_state != CLIENT_STATE_WAIT &&
        new_state != CLIENT_STATE_CHANGING && new_state != CLIENT_STATE_SHUT) {
            return CLIENT_MANAGER_ERR_INVALID;
    }

    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    _clients[fdp]->setState(new_state);

    return CLIENT_MANAGER_OK;
}

int ClientManager::updateClientKMin(FdPair *fdp, int k_min) {
    if (k_min < 0) {
        return CLIENT_MANAGER_ERR_INVALID;
    }

    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    _clients[fdp]->setKMin(k_min);

    return CLIENT_MANAGER_OK;
}

bool ClientManager::empty() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    return _clients.empty();
}

int ClientManager::getNumberClients() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    return connectedClients();
}

int ClientManager::size() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    return _clients.size();
}

int ClientManager::getTotalDataFrames() {
    int total = 0;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    for (std::pair<FdPair*, Client*> x : _clients) {
        total += x.second->getTotalDataFrames();
    }
    return total;
}

int ClientManager::getTotalCtrlFrames() {
    int total = 0;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    for (std::pair<FdPair*, Client*> x : _clients) {
        total += x.second->getTotalCtrlFrames();
    }
    return total;
}

int ClientManager::getTotalRecpFrames() {
    int total = 0;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    for (std::pair<FdPair*, Client*> x : _clients) {
        total += x.second->getTotalReceptionFrames();
    }
    return total;
}

int ClientManager::getClientState(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getState();
}

int ClientManager::getClientKMin(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    return _clients[fdp]->getKMin();
}

std::set<FdPair*> ClientManager::getBrokenClients() {
    std::set<FdPair*> broken_clients;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    int connected_clients = connectedClients();
    for (std::pair<FdPair*, Client*> x : _clients) {
        if (x.second->getState() == CLIENT_STATE_ACTIVE &&
            connected_clients < x.second->getKMin()) {
            broken_clients.insert(x.first);
        }
    }
    return broken_clients;
}

std::set<FdPair*> ClientManager::getFulfilledClients() {
    std::set<FdPair*> fulfilled_clients;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    int connected_clients = connectedClients();
    for (std::pair<FdPair*, Client*> x : _clients) {
        if (x.second->getState() == CLIENT_STATE_ACTIVE &&
           connected_clients >= x.second->getKMin()) {
            fulfilled_clients.insert(x.first);
        }
    }
    return fulfilled_clients;
}

std::set<FdPair*> ClientManager::getWaitingFulfilledClients() {
    std::set<FdPair*> fulfilled_clients;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    int connected_clients = connectedClients();
    for (std::pair<FdPair*, Client*> x : _clients) {
        if (x.second->getState() == CLIENT_STATE_WAIT &&
           connected_clients >= x.second->getKMin()) {
            fulfilled_clients.insert(x.first);
        }
    }
    return fulfilled_clients;
}

bool ClientManager::isClientBroken(FdPair *fdp) {
    assert(_clients.find(fdp) != _clients.end());
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    int connected_clients = connectedClients();
    int client_k_min  = _clients[fdp]->getKMin();
    return connected_clients < client_k_min;
}

int ClientManager::connectedClients() {
    int total = 0;
    for (std::pair<FdPair*, Client*> x : _clients) {
        if (VALID_CLIENT(x.second)) {
            total++;
        }
    }
    return total;
}

bool ClientManager::handleReceptionFrames(std::function<void(FdPair*, Client*)> f) {
    bool received_all = true;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.empty() == false);

    for (std::pair<FdPair*, Client*> x : _clients) {
        if (VALID_CLIENT(x.second)) {
            received_all = received_all && x.second->receivedFrame();
            if (!received_all)
                return false;
        }
    }

    for (std::pair<FdPair*, Client*> x : _clients) {
        if (VALID_CLIENT(x.second))
            f(x.first, x.second);
    }

    return true;
}

void ClientManager::setReceptionMark(FdPair *fdp) {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    assert(_clients.find(fdp) != _clients.end());
    _clients[fdp]->setReceptionMark();
}

Client* ClientManager::getClientInstance() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);

    for (std::pair<FdPair*, Client*> x : _clients) {
        return x.second;
    }

    return nullptr;
}

int ClientManager::unallocFramesFromClient(Client *client, FramePool *frame_pool) {
    FrameQueue *queue;
    Frame *frame;
    int status = 0;

    /* Unalloc frame lefthovers from shutting down clients */

    queue = client->getCtrlQueue();
    while (!queue->empty()) {
        frame = queue->getFrame();
        queue->pop();
        status += frame_pool->unallocFrame(frame);
    }

    queue = client->getDataQueue();
    while (!queue->empty()) {
        frame = queue->getFrame();
        queue->pop();
        status += frame_pool->unallocFrame(frame);
    }

    queue = client->getReceptionQueue();
    while (!queue->empty()) {
        frame = queue->getFrame();
        queue->pop();
        status += frame_pool->unallocFrame(frame);
    }

    return status;
}
