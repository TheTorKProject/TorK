#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>

#include "ControllerClient.hh"
#include "TrafficShaper.hh"
#include "../common/Common.hh"
#include "../tordriver/SocksProxyClient.hh"
#include "../tordriver/TorController.hh"
#include "../tordriver/TorPTClient.hh"
#include "../cli/CliUnixServer.hh"

#define BUFSIZE (4096)


ControllerClient::ControllerClient(int max_chunks, int chunk_size,
                                   int ts_min_rate, int ts_max_rate, int k_min,
                                   bool ch_active, bool abort_on_conn,
                                   TorPTClient *pt, SocksProxyClient *sp,
                                   TorController *tc, CliUnixServer *cli,
                                   TrafficShaper *ts)
    : _max_chunks(max_chunks), _chunk_size(chunk_size), _ts_min_rate(ts_min_rate),
      _ts_max_rate(ts_max_rate), _k_min(k_min), _ch_active_startup(ch_active),
      _abort_on_conn(abort_on_conn), _frame_pool(20, max_chunks, chunk_size),
      _chaff_frame(1, chunk_size)
{
    assert(sp != NULL && cli != NULL && ts != NULL);
    _pt = pt;
    _sp = sp;
    _tc = tc;
    _cli = cli;
    _ts = ts;

    _chaff_frame.setFrameType(FRAME_TYPE_CHAFF);
    int status = _chaff_frame.setChaffFrameData();
    assert(status == FRAME_OK);
}


void ControllerClient::config(int socks_port, int torctl_port)
{
    _socks_port = socks_port;
    _torctl_port = torctl_port;
}

void ControllerClient::handleSocksNewConnection(FdPair *fdp)
{
    #if (LOG_VERBOSE & LOG_BIT_CONN)
    _sp->log("Handling new connection (client: %d, bridge: %d)",
             fdp->get_fd0(),fdp->get_fd1());
    #endif

    assert(_client_manager.empty());

    _client_manager.add_client(fdp);

    /* Load client with channel and k_min passed through cmd args */
    if (_k_min != -1)
        announce_k();
}

void ControllerClient::handleSocksRestoreConnection(FdPair *fdp) {

    #if (LOG_VERBOSE & LOG_BIT_CTRL_LOCK)
        _sp->log("Restored connection (client: %d, bridge: %d)",
                fdp->get_fd0(),fdp->get_fd1());
    #endif
}

#define TORK_PROTOCOL

void ControllerClient::handleSocksClientDataReady(FdPair *fdp)
{
#ifdef TORK_PROTOCOL

    int stat = 0;
    int nread, space_sz;
    char *din_ptr;
    Frame *frame;

    if (_frame_pool.allocFrame(frame) == FRAME_POOL_ERR_FULL) {
        #if (LOG_VERBOSE & LOG_BIT_CONN)
            _sp->log("ClientDataReady: Frame Pool Full!");
        #endif
        return;
    }

    frame->setFrameType(FRAME_TYPE_DATA);
    stat += frame->getDataFrameSpace(din_ptr, space_sz);

    nread = _sp->read_msg_client(fdp, din_ptr, space_sz);
    if (nread <= 0) {
        _frame_pool.unallocFrame(frame);
        #if (LOG_VERBOSE & LOG_BIT_CONN)
            _sp->log("ClientDataReady: Client closed!");
        #endif
        _sp->shutdown_local_connection(fdp);
        return;
    }

    #if STATS
        _stats.add_tor_bytes_rec(nread);
    #endif

    stat += frame->setDataFrameSize(nread);
    assert(stat == FRAME_OK);

    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Read from client bytes (%d)", nread);
    #endif

    //if (_client_manager.getClientState(fdp) != CLIENT_STATE_INACTIVE) {
        _client_manager.getDataQueue(fdp)->push(frame);

    /*}
    else {
        #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
            _sp->log("Dropping frame from local to bridge since state is INACTIVE!");
        #endif

        stat = _frame_pool.unallocFrame(frame);
        assert(stat == FRAME_OK);
    }*/


#else

    int nread, nwrite;
    char buffer_r[BUFSIZE];

    nread = _sp->read_msg_client(fdp, buffer_r, BUFSIZE);
    if (nread <= 0) {
        _sp->shutdown_local_connection(fdp);
        return;
    }

    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Read from client bytes (%d)", nread);
    #endif

    nwrite = _sp->writen_msg_bridge(fdp, buffer_r, nread);
    if (nwrite <= 0) {
        _sp->shutdown_connection(fdp);
        return;
    }
#endif
}


void ControllerClient::handleSocksBridgeDataReady(FdPair *fdp)
{
    int nread, nwrite, status;
    Frame *frame;
    char *din_ptr; int din_sz, chunk, total_chunks = 1;
    bool ssl_try = false;
    FrameControlFields fcf;

    #if USE_SSL
        chunk = _client_manager.getTmpChunk(fdp);

        if (chunk == -1) {
            if (_frame_pool.allocFrame(frame) == FRAME_POOL_ERR_FULL) {
                #if (LOG_VERBOSE & LOG_BIT_CONN)
                    _sp->log("BridgeDataReady: Frame Pool Full!");
                #endif
                return;
            }
            chunk = 0;
        } else {
            ssl_try = true;
            frame = _client_manager.getTmpFrame(fdp);
            assert(frame != nullptr);
            if (chunk > 0)
                total_chunks = frame->getNumChunks();
        }
    #else
        if (_frame_pool.allocFrame(frame) == FRAME_POOL_ERR_FULL) {
            #if (LOG_VERBOSE & LOG_BIT_CONN)
                _sp->log("ClientDataReady: Frame Pool Full!");
            #endif
            return;
        }
        chunk = 0;
    #endif

    for (; chunk < total_chunks; chunk++)
    {
        frame->probeChunk(chunk, din_ptr, din_sz);

        nread = _sp->readn_msg_bridge(fdp, din_ptr, din_sz);
        if (nread <= 0) {
            #if (LOG_VERBOSE & LOG_BIT_CONN)
                _sp->log("BridgeDataReady: Bridge closed!");
            #endif
            if (nread != SSL_TRY_LATER) {
                _sp->shutdown_connection(fdp);
                status = _frame_pool.unallocFrame(frame);
                assert(status == FRAME_POOL_OK);
            } else {
                _client_manager.setTmpFrame(fdp, frame);
                _client_manager.setTmpChunk(fdp, chunk);
            }
            return;
        }
        assert(nread == din_sz);

        if (chunk == 0) {
            total_chunks = frame->getNumChunks();
            assert(total_chunks > 0 && total_chunks <= _max_chunks);
        }
    }

    //received complete frame from ssl, so clear tmp frame
    if (ssl_try) {
        _client_manager.setTmpChunk(fdp, -1);
        _client_manager.setTmpFrame(fdp, nullptr);
    }

    switch (frame->getFrameType()) {
        case FRAME_TYPE_DATA: break;
        case FRAME_TYPE_CHAFF:
            status = _frame_pool.unallocFrame(frame);
            assert(status == FRAME_OK);

            #if STATS
                #if DEBUG_TOOLS
                    if (!(_debug_info.getDropType() & DT_DROP_CHAFF))
                        _stats.add_no_data_bytes_rec(nread);
                #else
                    _stats.add_no_data_bytes_rec(nread);
                #endif
            #endif
        return;
        case FRAME_TYPE_CTRL:
            status = frame->getCtrlFrameData(fcf);
            assert(status == FRAME_OK);

            #if STATS
                #if DEBUG_TOOLS
                    if (!(_debug_info.getDropType() & DT_DROP_CTRL)) {
                        _stats.add_no_data_bytes_rec(nread);
                    }
                    else {
                        status = _frame_pool.unallocFrame(frame);
                        assert(status == FRAME_OK);
                        return;
                    }
                #else
                    _stats.add_no_data_bytes_rec(nread);
                #endif
            #endif

            #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                _sp->log("Received Control frame:");
                frame->printFrameInfo(std::cerr);
            #endif

            switch (fcf._type) {
                case FRAME_CTRL_TYPE_NULL        : handleCtrlFrame_NULL(fdp);
                break;
                case FRAME_CTRL_TYPE_HELLO       : handleCtrlFrame_HELLO(fdp, fcf);
                break;
                case FRAME_CTRL_TYPE_HELLO_OK    : handleCtrlFrame_HELLO_OK(fdp);
                break;
                case FRAME_CTRL_TYPE_ACTIVE      : handleCtrlFrame_ACTIVE(fdp);
                break;
                case FRAME_CTRL_TYPE_WAIT        : handleCtrlFrame_WAIT(fdp);
                break;
                case FRAME_CTRL_TYPE_CHANGE      : handleCtrlFrame_CHANGE(fdp);
                break;
                case FRAME_CTRL_TYPE_CHANGE_OK   : handleCtrlFrame_CHANGE_OK(fdp);
                break;
                case FRAME_CTRL_TYPE_INACTIVE    : handleCtrlFrame_INACTIVE(fdp);
                break;
                case FRAME_CTRL_TYPE_SHUT        : handleCtrlFrame_SHUT(fdp);
                break;
                case FRAME_CTRL_TYPE_SHUT_OK     : handleCtrlFrame_SHUT_OK(fdp);
                break;
                case FRAME_CTRL_TYPE_TS_RATE     : handleCtrlFrame_TS_RATE(fdp, fcf);
                break;
                case FRAME_CTRL_TYPE_ERR_HELLO   : handleCtrlFrame_ERR_HELLO(fdp);
                break;
                case FRAME_CTRL_TYPE_ERR_ACTIVE  : handleCtrlFrame_ERR_ACTIVE(fdp);
                break;
                case FRAME_CTRL_TYPE_ERR_INACTIVE: handleCtrlFrame_ERR_INACTIVE(fdp);
                break;
                default                          : handleCtrlFrame_UNKNOWN(fdp);
            }

            status = _frame_pool.unallocFrame(frame);
            assert(status == FRAME_OK);
            return;

        default:
            #if (LOG_VERBOSE & LOG_BIT_CONN)
                _sp->log("Wrong frame type, dropping frame. %d", frame->getFrameType());
            #endif
            status = _frame_pool.unallocFrame(frame);
            assert(status == FRAME_OK);
            return;
    }

    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Read from bridge num bytes (%d)", nread);
    #endif

    char *dout_ptr; int dout_sz;
    status = frame->getDataFrameData(dout_ptr, dout_sz);
    assert(status == FRAME_OK);

    #if STATS
        #if DEBUG_TOOLS
            if (!(_debug_info.getDropType() & DT_DROP_DATA))
                _stats.add_bytes_rec(dout_sz);
        #else
            _stats.add_bytes_rec(dout_sz);
        #endif
    #endif

    #if DEBUG_TOOLS
        nwrite = DT_CONTROL(_sp->write_msg_client(fdp, dout_ptr, dout_sz),
            DT_DROP_DATA, _debug_info, dout_sz);
    #else
        nwrite = _sp->write_msg_client(fdp, dout_ptr, dout_sz);
    #endif

    status = _frame_pool.unallocFrame(frame);
    assert(status == FRAME_OK);

    if (nwrite <= 0) {
        #if (LOG_VERBOSE & LOG_BIT_CONN)
            _sp->log("BridgeDataReady: Failed to write to client!");
        #endif
        _sp->shutdown_local_connection(fdp);
        return;
    }

    assert(nwrite == dout_sz);

    #if STATS
        #if DEBUG_TOOLS
            if (!(_debug_info.getDropType() & DT_DROP_DATA))
                _stats.add_tor_bytes_sent(dout_sz);
        #else
            _stats.add_tor_bytes_sent(dout_sz);
        #endif
    #endif
}


void ControllerClient::handleSocksConnectionTerminated(FdPair *fdp)
{
    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Terminated connection (client: %d, bridge: %d)",
                fdp->get_fd0(), fdp->get_fd1());
    #endif

    _client_manager.remove_client(fdp, &_frame_pool);

    if (_abort_on_conn)
        abort();
}


void ControllerClient::handleSocksNewSessionError()
{
    #if (LOG_VERBOSE & LOG_BIT_CONN & LOG_BIT_ERRORS)
        _sp->log("Error establishing socks session");
    #endif
}


void ControllerClient::handleTorCtlInitialized()
{
    #if (LOG_VERBOSE & LOG_BIT_CONN & LOG_BIT_CTRL)
        _tc->log("Tor controller initialized");
    #endif
}


void ControllerClient::handleTorCtlEventReceived(TorEvent *event)
{
    assert(event != NULL);
    int status;

    if (event->_type == TCTL_EVENT_STREAM_NEW) {
        #if (LOG_VERBOSE & LOG_BIT_CTRL)
            _tc->log("Tor controller event -- new stream: %d",
                    event->_stream);
        #endif

        if (_circ_state == CIRC_STATE_BUILT) {
            #if (LOG_VERBOSE & LOG_BIT_CTRL)
                _tc->log("Tor controller event -- attaching to circuit: %d",
                _circ);
            #endif

            status = _tc->cmdAttachStream(event->_stream, _circ);
            if (status == TORCTL_CMD_OK) {
                #if (LOG_VERBOSE & LOG_BIT_CTRL)
                    _tc->log("Tor controller event -- stream attach successful.\n");
                #endif
            } else {
                #if (LOG_VERBOSE & LOG_BIT_CTRL)
                    _tc->log("Tor controller event -- stream attach failed.\n");
                #endif
            }
        } else {
            #if (LOG_VERBOSE & LOG_BIT_CTRL)
                _tc->log("Tor controller event -- no circuit available.\n");
            #endif
            _pending_streams.insert(event->_stream);
        }
        return;
    }

    if (event->_type == TCTL_EVENT_CIRC_BUILT) {
        #if (LOG_VERBOSE & LOG_BIT_CTRL)
            _tc->log("Tor controller event -- circ built: %d", event->_circ);
        #endif

        //Attach pending streams, if any
        if (_circ == event->_circ){
            _circ_state = CIRC_STATE_BUILT;
            _circ_attempts = 0;

            for (int pending_stream : _pending_streams) {
                status = _tc->cmdAttachStream(pending_stream, _circ);
                if (status == TORCTL_CMD_OK) {
                    #if (LOG_VERBOSE & LOG_BIT_CTRL)
                        _tc->log("Tor controller event -- auto stream %d attach \
successful.\n", pending_stream);
                    #endif
                } else {
                    #if (LOG_VERBOSE & LOG_BIT_CTRL)
                        _tc->log("Tor controller event -- auto stream %d attach \
failed.\n", pending_stream);
                    #endif
                }
            }
        }

        return;
    }

    if (event->_type == TCTL_EVENT_CIRC_FAILED) {
        if (_circ == event->_circ) {
            #if (LOG_VERBOSE & LOG_BIT_CTRL)
                if (_circ_attempts > 0) {
                    _tc->log("Tor controller event -- circ failed for the %d time(s): %d",
                        _circ_attempts, event->_circ);
                }
                else {
                _tc->log("Tor controller event -- circ failed: %d",
                        event->_circ);
                }

            #endif
            _circ = NO_CIRCUIT;
            _circ_state = CIRC_STATE_UNDEF;

            if (!create_circuit()) {
                //failed all attempts to create a circuit
                #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                    _sp->log("FAILED ALL %d time(s) attmps to create new circuit. \
    Sending INACTIVE to the bridge.",
                    _circ_attempts);
                #endif
                Frame *frame;
                FrameControlFields fcf;

                status = _frame_pool.allocFrame(frame);
                assert(status == FRAME_OK);

                frame->setFrameType(FRAME_TYPE_CTRL);

                fcf._type = FRAME_CTRL_TYPE_INACTIVE;
                status = frame->setCtrlFrameData(&fcf);
                assert(status == FRAME_OK);

                _client_manager.safeIterate([frame](FdPair* fdp, Client* client) {
                    client->setState(CLIENT_STATE_INACTIVE);
                    client->getCtrlQueue()->push(frame);
                });
            }
        }
        return;
    }

    if (event->_type == TCTL_EVENT_CIRC_CLOSED) {

        if (_circ == event->_circ) {
            #if (LOG_VERBOSE & LOG_BIT_CTRL)
                _tc->log("Tor controller event -- circ closed: %d",
                        event->_circ);
            #endif

            _circ = NO_CIRCUIT;
            _circ_state = CIRC_STATE_UNDEF;

        }
        return;
    }

    if (event->_type == TCTL_EVENT_STREAM_CLOSED) {
        #if (LOG_VERBOSE & LOG_BIT_CTRL)
            _tc->log("Tor controller event -- stream closed: %d",
                    event->_stream);
        #endif
        _pending_streams.erase(event->_stream);
        return;
    }

    if (event->_type == TCTL_EVENT_STC_ENOUGH_DIR_INFO) {
        #if (LOG_VERBOSE & LOG_BIT_CTRL)
            _tc->log("Tor controller event -- loaded enough dir info!");
        #endif

        _dir_info = true;

        if (_circ == WAITING_DIR_INFO) {
            if (create_circuit()) {
                #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                    _sp->log("Successfully CREATE new circuit %d after DIR INFO.",
                    _circ);
                #endif
            } else {
                #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                _sp->log("Failed to open new circuit after DIR INFO");
                #endif
            }
        }

        return;
    }

    #if (LOG_VERBOSE & LOG_BIT_CTRL_EVENTS)
        _tc->log("Tor controller event: %s", event->_descr.c_str());
    #endif

}


void ControllerClient::handleCliRequest(std::string &request, std::string &response)
{
    int status = -1;
    std::string cmd;
    Frame* frame_to_send;
    FrameControlFields fcf;

    response.clear();

    if (request.empty()) {
        response = "\n";
        return;
    }

    std::istringstream request_stream(request);
    std::vector<std::string> params(std::istream_iterator<std::string>(request_stream), {});

    if (params.empty()) {
        response = "\n";
        return;
    } else {
        cmd = params[0];
    }

    #if (LOG_VERBOSE & LOG_BIT_CLI)
        std::cerr << "Cli request: " << cmd << "." << std::endl;
    #endif

    if (cmd == "stats_fp") {
        response = (boost::format("%d\t%d\t%d\n")
                % _frame_pool.getNumAllocFrames()
                % _frame_pool.getNumUnallocFrames()
                % _frame_pool.size()).str();
        return;
    }

    if (cmd == "stats") {
        Client *client = _client_manager.getClientInstance();
        assert(client != nullptr);

        response = (boost::format("%d\t%d\t%d\t%d\t%d\n")
                    % client->getState()
                    % client->getKMin()
                    % client->getTotalCtrlFrames()
                    % client->getTotalDataFrames()
                    % client->getTotalReceptionFrames()).str();
        return;
    }

    if (cmd == "ts") {
        response = (boost::format("%d\n")
                    % _ts->getRate()).str();
        return;
    }

    #if STATS
        if (cmd == "stats_bytes") {
            Client *client = _client_manager.getClientInstance();
            int state = (client == nullptr) ? CLIENT_STATE_NOT_CONN : client->getState();

            response = (boost::format("%ld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n")
                    % time(NULL)
                    % _stats.getCBytesReceived()
                    % _stats.getCBytesSent()
                    % _stats.getCTorBytesReceived()
                    % _stats.getCTorBytesSent()
                    % _stats.getCNoDataReceived()
                    % _stats.getCNoDataSent()
                    % state).str();
            return;
        }
    #endif

    if (cmd == "nym") {
        /* Force Tor to clean circuit and create a new one. */
        _tc->cmdSendSignal(TCTL_SIGNAL_NEWNYM);
        if (!create_circuit()) {
            //failed all attempts to create a circuit
            #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                _sp->log("FAILED ALL attmps to create new circuit. \
                After receiving nym signal.");
            #endif
            response = "FAIL\n";
        } else {
            response = "OK\n";
        }
        return;
    }

    if (cmd == "pend_streams") {
        response = "PS: ";
        for (int pending_stream : _pending_streams) {
            response = response + std::to_string(pending_stream) + " ";
        }
        response += "\n";
        return;
    }

    if (cmd == "v") {
        std::string version;
        status = _tc->cmdGetInfoVersion(version);
        if (status == TORCTL_CMD_OK) {
            response = version + "\n";
            return;
        } else {
            response = "cannot obtain the version information.\n";
            return;
        }
    }

    if (cmd == "s") {
        std::vector<std::string> streams;
        status = _tc->cmdGetInfoStreamStatus(streams);
        if (status == TORCTL_CMD_OK) {
            for(std::vector<std::string>::const_iterator it = streams.begin(); it != streams.end(); it++) {
                response += *it;
                response += "\n";
            }
            std::cerr << "Cli response: " << response.c_str() << "." << std::endl;
            return;
        } else {
            response = "cannot obtain the stream status information.\n";
            return;
        }
    }

    if (cmd == "sec") {
        status = _tc->cmdSetEventsCirc();
        if (status == TORCTL_CMD_OK) {
            response += "ok\n";
            return;
        } else {
            response = "cannot set events circ.\n";
            return;
        }
    }

    if (cmd == "ses") {
        status = _tc->cmdSetEventsStream();
        if (status == TORCTL_CMD_OK) {
            response += "ok\n";
            return;
        } else {
            response = "cannot set events stream.\n";
            return;
        }
    }

    if (cmd == "c") {

        if (create_circuit()) {
            #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                _sp->log("TCTL order open of circuit %d.",
                _circ);
            #endif
            response = "ok: circuit " + std::to_string(_circ) + ".\n";
            return;
        } else {
            #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                if (_circ == NO_CIRCUIT) {
                    _sp->log("TCTL failed to open circuit.");
                }
                else if (_circ == WAITING_DIR_INFO) {
                    _sp->log("TCTL postpone circuit open after DIR INFO.");
                }
            #endif
            response = "error (" + std::to_string(_circ) + ").\n";
            return;
        }
    }

    if (cmd == "set_k") {
        if (params.size() == 1) {
            _k_min = 3;
            response = "You did not provide the k_min, assuming default value k_min = 3\n";
        } else {
            _k_min = std::stoi(params[1]);
            announce_k();
        }
        return;
    }

    if (cmd == "ch") {
        if (params.size() != 2) {
            response = "ch (active | inactive).\n";
        } else {
            set_channel_status(params[1] == "active");
        }
        return;
    }

    if (cmd == "shut") {

        status = _frame_pool.allocFrame(frame_to_send);
        assert(status == FRAME_OK);

        frame_to_send->setFrameType(FRAME_TYPE_CTRL);

        fcf._type = FRAME_CTRL_TYPE_SHUT;
        status = frame_to_send->setCtrlFrameData(&fcf);
        assert(status == FRAME_OK);

        _client_manager.safeIterate([frame_to_send](FdPair* fdp, Client* client) {
            client->setState(CLIENT_STATE_SHUT);
            client->getCtrlQueue()->push(frame_to_send);
        });

        response = "Requested disconnect to the bridge.\n";
        return;
    }

    #if DEBUG_TOOLS
        if (cmd == "drop") {
            if (params.size() != 3) {
                response = "drop on (all | ctrl | data | chaff)\n"
                           "drop off\n";
            } else {
                if (params[1] == "on" && params[2] == "all") {
                    _debug_info.setDropType(DT_DROP_ALL);
                    response = "Drop ALL packets ON.\n";
                } else if (params[1] == "on" && params[2] == "ctrl") {
                    _debug_info.setDropType(DT_DROP_CTRL);
                    response = "Drop CTRL packets ON.\n";
                } else if (params[1] == "on" && params[2] == "data") {
                    _debug_info.setDropType(DT_DROP_DATA);
                    response = "Drop DATA packets ON.\n";
                } else if (params[1] == "on" && params[2] == "chaff") {
                    _debug_info.setDropType(DT_DROP_CHAFF);
                    response = "Drop CHAFF packets ON.\n";
                } else if (params[1] == "off") {
                    _debug_info.setDropType(DT_DROP_OFF);
                    response = "Drop ALL packets OFF.\n";
                }
            }
            return;
        }
    #endif

    response = "error - unknown command.\n";
}

void ControllerClient::handleTrafficShapingEvent()
{
    _client_manager.safeIterate([this](FdPair* fdp, Client* client) {
        FrameQueue* ctrl_frame_queue = client->getCtrlQueue();
        FrameQueue* data_frame_queue = client->getDataQueue();
        Frame* frame_to_send;
        int status, nwrite, chunk_sz, chunk;
        char* chunk_ptr;

        /* does last SSL write returned SSL_WANT_WRITE?
        If yes, resume frame type. */
        int ssl_partial_frame = client->getWRTmpFrameType();

        bool partial_data_frame = !data_frame_queue->empty() && data_frame_queue->getLastChunk() > 0;

        if ((ssl_partial_frame == -1 || ssl_partial_frame == FRAME_TYPE_CTRL) &&
            (!partial_data_frame && !ctrl_frame_queue->empty())) {
            frame_to_send = ctrl_frame_queue->getFrame();

            status = frame_to_send->probeChunk(0, chunk_ptr, chunk_sz);
            assert(status == FRAME_OK);

            #if DEBUG_TOOLS
                nwrite = DT_CONTROL(_sp->writen_msg_bridge(fdp, chunk_ptr,
                    chunk_sz), DT_DROP_CTRL, _debug_info, chunk_sz);
            #else
                nwrite = _sp->writen_msg_bridge(fdp, chunk_ptr, chunk_sz);
            #endif

            if (nwrite != SSL_TRY_LATER) {
                ctrl_frame_queue->pop();
                #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                    _sp->log("Popped from ctrl queue. Left %d ", ctrl_frame_queue->size());
                #endif
                _frame_pool.unallocFrame(frame_to_send);
            } else {
                client->setWRTmpFrameType(FRAME_TYPE_CTRL);
            }

            #if STATS
                if (nwrite > 0) {
                    _stats.add_no_data_bytes_sent(nwrite);
                }
            #endif

        } else if ((ssl_partial_frame == -1 || ssl_partial_frame == FRAME_TYPE_DATA) &&
                (!data_frame_queue->empty() && client->getState() == CLIENT_STATE_ACTIVE)) {
            frame_to_send = data_frame_queue->getFrame();
            chunk = data_frame_queue->getLastChunk();

            #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                _sp->log("Got frame from data queue.");
            #endif

            status = frame_to_send->probeChunk(chunk, chunk_ptr, chunk_sz);
            assert(status == FRAME_OK);

            #if DEBUG_TOOLS
                nwrite = DT_CONTROL(_sp->writen_msg_bridge(fdp, chunk_ptr,
                    chunk_sz), DT_DROP_DATA, _debug_info, chunk_sz);
            #else
                nwrite = _sp->writen_msg_bridge(fdp, chunk_ptr, chunk_sz);
            #endif

            if (nwrite != SSL_TRY_LATER) {
                if (chunk + 1 < frame_to_send->getNumChunks()) {
                    data_frame_queue->setLastChunk(chunk + 1);
                } else {
                    #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                        _sp->log("Popped from data queue. Left %d ", data_frame_queue->size());
                    #endif
                    data_frame_queue->pop();

                    #if STATS
                        if (nwrite > 0) {
                            char* data_ptr; int data_sz;
                            status = frame_to_send->getDataFrameData(data_ptr, data_sz);
                            assert(status == FRAME_OK);
                            _stats.add_bytes_sent(data_sz);
                        }
                    #endif

                    _frame_pool.unallocFrame(frame_to_send);
                }
            } else {
                client->setWRTmpFrameType(FRAME_TYPE_DATA);
            }


        } else {
            frame_to_send = &_chaff_frame;

            status = frame_to_send->probeChunk(0, chunk_ptr, chunk_sz);
            assert(status == FRAME_OK);

            #if DEBUG_TOOLS
                nwrite = DT_CONTROL(_sp->writen_msg_bridge(fdp, chunk_ptr,
                    chunk_sz), DT_DROP_CHAFF, _debug_info, chunk_sz);
            #else
                nwrite = _sp->writen_msg_bridge(fdp, chunk_ptr, chunk_sz);
            #endif

            if (nwrite == SSL_TRY_LATER) {
                client->setWRTmpFrameType(FRAME_TYPE_CHAFF);
            }

            #if STATS
                if (nwrite > 0) {
                    _stats.add_no_data_bytes_sent(nwrite);
                }
            #endif
        }

        if (nwrite <= 0) {
            #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                _sp->log("Failed to send!");
            #endif
        }
        else {
            assert(nwrite == chunk_sz);
        }
    });
}

/* ======================= CTRL Frames Handlers ======================= */

void ControllerClient::handleCtrlFrame_NULL(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_NULL))
        _sp->log("Received NULL frame from bridge. Ignoring.");
    #endif
}

void ControllerClient::handleCtrlFrame_HELLO(FdPair *fdp, FrameControlFields &fcf) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO))
    _sp->log("Received HELLO from bridge but bridges aren't supposed to send \
them. Ignoring.");
    #endif
}

void ControllerClient::handleCtrlFrame_HELLO_OK(FdPair *fdp) {
    int state;

    state = _client_manager.getClientState(fdp);

    if (state == CLIENT_STATE_HELLO) {
        _client_manager.updateClientState(fdp, CLIENT_STATE_CONNECTED);
        /* Request channel activation if passed through cmd args */
        if (_ch_active_startup)
            set_channel_status(_ch_active_startup);
    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO_OK))
    else {
        _sp->log("Received HELLO_OK but did not request HELLO. Ignored.");
    }
    #endif
}

void ControllerClient::handleCtrlFrame_ACTIVE(FdPair *fdp) {
    assert(_tc != nullptr);

    int status, state;

    state = _client_manager.getClientState(fdp);

    if (state == CLIENT_STATE_CONNECTED ||
        state == CLIENT_STATE_WAIT      ||
        state == CLIENT_STATE_INACTIVE) {

        _client_manager.updateClientState(fdp, CLIENT_STATE_ACTIVE);

        status = _tc->cmdSendSignal(TCTL_SIGNAL_ACTIVE);
        assert(status == TORCTL_CMD_OK);

        if (create_circuit()) {
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_ACTIVE))
                _sp->log("Received ACTIVE and order open of circuit %d.",
                _circ);
            #endif
        } else {
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_ACTIVE))
                if (_circ == NO_CIRCUIT) {
                    _sp->log("Received ACTIVE but failed to open circuit.");
                }
                else if (_circ == WAITING_DIR_INFO) {
                    _sp->log("Received ACTIVE and postpone circuit open after DIR INFO.");
                }
            #endif
        }

    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_ACTIVE))
    else if (state == CLIENT_STATE_UNDEF) {
        _sp->log("Received ACTIVE but did not even request HELLO. \
Ignored.");
    }
    else if (state == CLIENT_STATE_HELLO) {
        _sp->log("Received ACTIVE but did not even received HELLO OK. \
Ignored.");
    }
    else if (state == CLIENT_STATE_ACTIVE) {
        _sp->log("Received ACTIVE but I'm already ACTIVE. Ignored.");
    }
    else if (state == CLIENT_STATE_SHUT) {
        _sp->log("Received ACTIVE but I'm SHUT. Ignored.");
    }
    #endif
}

void ControllerClient::handleCtrlFrame_WAIT(FdPair *fdp) {
    int status, state;

    state = _client_manager.getClientState(fdp);

    if (state == CLIENT_STATE_CONNECTED ||
        state == CLIENT_STATE_ACTIVE    ||
        state == CLIENT_STATE_INACTIVE) {

        _client_manager.updateClientState(fdp, CLIENT_STATE_WAIT);

        status = shutdown_local_helper(fdp, NO_CIRCUIT);

        if (status == 0) {
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_WAIT))
                _sp->log("Received WAIT therefore connection kaput!");
            #endif
        }
        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_WAIT))
        else {
            _sp->log("Received WAIT but connection closure failed \
with code %d!", status);
        }
        #endif

    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_WAIT))
    else if (state == CLIENT_STATE_UNDEF) {
        _sp->log("Received WAIT but did not even request HELLO. \
Ignored.");
    }
    else if (state == CLIENT_STATE_HELLO) {
        _sp->log("Received WAIT but did not even received HELLO OK. \
Ignored.");
    }
    else if (state == CLIENT_STATE_WAIT) {
        _sp->log("Received WAIT but I'm already WAIT. Ignored.");
    }
    else if (state == CLIENT_STATE_SHUT) {
        _sp->log("Received WAIT but I'm on SHUT. Ignored.");
    }
    #endif
}

void ControllerClient::handleCtrlFrame_CHANGE(FdPair *fdp) {
    assert(_tc != nullptr);

    int status, state;
    Frame *reply;
    FrameControlFields reply_fcf;

    state = _client_manager.getClientState(fdp);

    if (state == CLIENT_STATE_ACTIVE) {

        _client_manager.updateClientState(fdp, CLIENT_STATE_CHANGING);

        status = shutdown_local_helper(fdp, CHANGE_CIRCUIT);

        if (status == 0) {
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE))
                _sp->log("Received CHANGE therefore connection kaput!");
            #endif

            // Send CHANGE OK, notifying that the current circuit was
            // terminated.
            status = _frame_pool.allocFrame(reply);
            assert(status == FRAME_OK);

            reply->setFrameType(FRAME_TYPE_CTRL);

            reply_fcf._type = FRAME_CTRL_TYPE_CHANGE_OK;
            status = reply->setCtrlFrameData(&reply_fcf);
            assert(status == FRAME_OK);

            _client_manager.updateClientState(fdp, CLIENT_STATE_ACTIVE);
            _client_manager.getCtrlQueue(fdp)->push(reply);

            status = _tc->cmdSendSignal(TCTL_SIGNAL_ACTIVE);
            assert(status == TORCTL_CMD_OK);

            //construct a new circuit
            if (create_circuit()) {
                #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                    (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE))
                    _sp->log("Successfully CHANGE to new circuit %d.",
                    _circ);
                #endif
            } else {
                #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                    (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE))
                    if (_circ == NO_CIRCUIT) {
                        _sp->log("CHANGE failed to open new circuit.");
                    }
                    else if (_circ == WAITING_DIR_INFO) {
                        _sp->log("CHANGE postpone circuit open after DIR INFO.");
                    }
                #endif
            }

        }
        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE))
        else {
            _sp->log("Received CHANGE but connection closure failed %d!", status);
        }
        #endif



    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE))
    else if (state == CLIENT_STATE_UNDEF) {
        _sp->log("Received CHANGE but did not even request HELLO. \
Ignored.");
    }
    else if (state == CLIENT_STATE_HELLO) {
        _sp->log("Received CHANGE but expected HELLO OK. \
Ignored.");
    }
    else if (state == CLIENT_STATE_CONNECTED) {
        _sp->log("Received CHANGE but not request ACTIVE. Ignored.");
    }
    else if (state == CLIENT_STATE_WAIT) {
        _sp->log("Received CHANGE but I'm on WAIT. Ignored.");
    }
    else if (state == CLIENT_STATE_CHANGING) {
        _sp->log("Received CHANGE but I'm already CHANGING. Ignored.");
    }
    else if (state == CLIENT_STATE_INACTIVE) {
        _sp->log("Received CHANGE but not request ACTIVE. Ignored.");
    }
    else if (state == CLIENT_STATE_SHUT) {
        _sp->log("Received CHANGE but I'm on SHUT. Ignored.");
    }
    #endif
}

void ControllerClient::handleCtrlFrame_CHANGE_OK(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
    (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE_OK))
        _sp->log("Received CHANGE OK from bridge but bridges aren't supposed \
to send them. Ignoring.");
    #endif
}

void ControllerClient::handleCtrlFrame_INACTIVE(FdPair *fdp) {
    int status, state, circuitID;

    state = _client_manager.getClientState(fdp);
    _client_manager.updateClientState(fdp, CLIENT_STATE_INACTIVE);

    if (state == CLIENT_STATE_ACTIVE ||
        state == CLIENT_STATE_WAIT) {

        if (_circ > 0) {
            circuitID = _circ;

            status = shutdown_local_helper(fdp, NO_CIRCUIT);

            if (status == 0) {
                #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                    (LOG_CTRL_TYPES & LOG_BIT_TYPE_INACTIVE))
                    _sp->log("Received INACTIVE therefore circuit %d \
kaput!", circuitID);
                #endif
            }
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_INACTIVE))
            else {
                _sp->log("Received INACTIVE but circuit %d closure \
failed with code %d!", circuitID, status);
            }
            #endif

        }
        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_INACTIVE))
        else {
            if (state == CLIENT_STATE_ACTIVE)
                _sp->log("Received INACTIVE being ACTIVE but circuitID \
was empty.");
            else if (state == CLIENT_STATE_WAIT)
                _sp->log("Received INACTIVE being WAIT therefore gave \
up of waiting.");
        }
        #endif

    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_INACTIVE))
    else if (state == CLIENT_STATE_UNDEF) {
        _sp->log("Received INACTIVE but did not even request HELLO. \
Ignored.");
    }
    else if (state == CLIENT_STATE_HELLO) {
        _sp->log("Received INACTIVE but did not even received HELLO OK. \
Ignored.");
    }
    else if (state == CLIENT_STATE_INACTIVE) {
        _sp->log("Received INACTIVE but I'm already INACTIVE. Ignored.");
    }
    else if (state == CLIENT_STATE_SHUT) {
        _sp->log("Received INACTIVE but I'm on SHUT. Ignored.");
    }
    #endif
}

void ControllerClient::handleCtrlFrame_SHUT(FdPair *fdp) {
    int status;

    _client_manager.updateClientState(fdp, CLIENT_STATE_SHUT);

    if (_circ > 0) {
        status = shutdown_local_helper(fdp, NO_CIRCUIT);

        if (status == 0) {
        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT))
                _sp->log("Received SHUT OK frame from bridge. \
Destroying connection");
            #endif

        }
        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT))
        else {
            _sp->log("Received SHUT OK but circuit %d closure \
failed with code %d!", _circ, status);
        }
        #endif

    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT))
    else {
        _sp->log("Received SHUT OK frame from bridge. \
No circuit to destroy.");
    }
    #endif
}

void ControllerClient::handleCtrlFrame_SHUT_OK(FdPair *fdp) {
    assert(_tc != nullptr);

    int status, circuitID;

    _client_manager.updateClientState(fdp, CLIENT_STATE_SHUT);

    if (_circ > 0) {
        circuitID = _circ;
        status = shutdown_local_helper(fdp, NO_CIRCUIT);

        if (status == 0) {
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT_OK))
                _sp->log("Received SHUT OK frame from bridge. \
Destroying circuit %d and connection.", circuitID);
            #endif

        }
            #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
                (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT_OK))
        else {
            _sp->log("Received SHUT OK but circuit %d closure \
failed with code %d!", circuitID, status);
        }
        #endif

    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT_OK))
    else {
        _sp->log("Received SHUT OK frame from bridge. \
No circuit to destroy.");
    }
    #endif

    status = _tc->cmdSendSignal(TCTL_SIGNAL_SHUTDOWN);
    assert(status == FRAME_OK);

}

void ControllerClient::handleCtrlFrame_TS_RATE(FdPair *fdp, FrameControlFields &fcf) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_TS_RATE))
        _sp->log("Received TS_RATE from bridge with rate value %d", fcf._ts_rate);
    #endif
    assert(fcf._ts_rate >= _ts_min_rate && fcf._ts_rate <= _ts_max_rate);
    _ts->setRate(fcf._ts_rate);
}

void ControllerClient::handleCtrlFrame_ERR_HELLO(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO))
        _sp->log("Received ERR_HELLO from bridge.");
    #endif
}

void ControllerClient::handleCtrlFrame_ERR_ACTIVE(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_ACTIVE))
        _sp->log("Received ERR_ACTIVE from bridge.");
    #endif
}

void ControllerClient::handleCtrlFrame_ERR_INACTIVE(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_INACTIVE))
        _sp->log("Received ERR_INACTIVE from bridge.");
    #endif
}

void ControllerClient::handleCtrlFrame_UNKNOWN(FdPair *fdp) {
    #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
        _sp->log("Received Unknown ctrl frame. Ignoring.");
    #endif
}

bool ControllerClient::create_circuit() {
    assert(_tc != nullptr);

    int status, circuitID;

    if (!_dir_info) {
        _circ = WAITING_DIR_INFO;
        return false;
    }

    while (_circ_attempts <= CIRC_RETRY_ATMPS) {
        status = _tc->cmdExtendCircuit(circuitID);

        if (status == TORCTL_CMD_OK) {
            _circ = circuitID;
            _circ_state = CIRC_STATE_UNDEF;
            return true;
        } else {
            #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                _sp->log("Failed %d time(s) to create new circuit. Error %d",
                _circ_attempts, status);
            #endif
            _circ = NO_CIRCUIT;
            _circ_state = CIRC_STATE_UNDEF;
            _circ_attempts++;
        }
    }

    return false;
}

int ControllerClient::shutdown_local_helper(FdPair *fdp, int circ_val) {

    assert(fdp != nullptr && circ_val < 0);

    int status;
    FrameQueue* data_queue = _client_manager.getDataQueue(fdp);
    Frame* frame;

    _circ = circ_val;
    _circ_state = CIRC_STATE_UNDEF;

    if (_tc != nullptr) {
        status = _tc->cmdSendSignal(TCTL_SIGNAL_DORMANT);
        assert(status == TORCTL_CMD_OK);
    }

    status = _sp->shutdown_local_connection(fdp);

    while (!data_queue->empty()) {
        frame = data_queue->getFrame();
        data_queue->pop();
        _frame_pool.unallocFrame(frame);
    }

    #if (LOG_VERBOSE & LOG_BIT_CTRL_LOCK)
        _sp->log("Shutdown local connection: bridge %d", fdp->get_fd1());
    #endif

    return status;
}

void ControllerClient::announce_k() {
    assert(_k_min >= 0);

    int status;
    FrameControlFields fcf;
    Frame* frame_to_send;

    fcf._k_min = _k_min;

    if (_tc != nullptr) {
        status = _tc->cmdSendSignal(TCTL_SIGNAL_ACTIVE);
        assert(status == TORCTL_CMD_OK);
    }

    status = _frame_pool.allocFrame(frame_to_send);
    assert(status == FRAME_OK);

    frame_to_send->setFrameType(FRAME_TYPE_CTRL);


    fcf._type = FRAME_CTRL_TYPE_HELLO;
    status = frame_to_send->setCtrlFrameData(&fcf);
    assert(status == FRAME_OK);

    _client_manager.safeIterate([frame_to_send, fcf](FdPair* fdp, Client* client) {
        client->setState(CLIENT_STATE_HELLO);
        client->setKMin(fcf._k_min);
        client->getCtrlQueue()->push(frame_to_send);
    });
}

void ControllerClient::set_channel_status(bool enabled) {
    assert(_tc != nullptr);

    int status;
    FrameControlFields fcf;
    Frame* frame_to_send;

    status = _frame_pool.allocFrame(frame_to_send);
    assert(status == FRAME_OK);

    frame_to_send->setFrameType(FRAME_TYPE_CTRL);

    if (enabled) {
        status = _tc->cmdSendSignal(TCTL_SIGNAL_ACTIVE);
        assert(status == TORCTL_CMD_OK);
        fcf._type = FRAME_CTRL_TYPE_ACTIVE;

    } else {
        fcf._type = FRAME_CTRL_TYPE_INACTIVE;
    }

    status = frame_to_send->setCtrlFrameData(&fcf);
    assert(status == FRAME_OK);

    _client_manager.safeIterate([frame_to_send](FdPair* fdp, Client* client) {
        client->getCtrlQueue()->push(frame_to_send);
    });
}
