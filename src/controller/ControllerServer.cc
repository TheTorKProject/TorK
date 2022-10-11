#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

#include <boost/algorithm/string/trim.hpp>
#include <boost/format.hpp>

#include "FdPair.hh"
#include "ControllerServer.hh"
#include "../common/Common.hh"
#include "../tordriver/SocksProxyServer.hh"
#include "../tordriver/TorPTServer.hh"
#include "../cli/CliUnixServer.hh"

#if TIME_STATS
    #include <chrono>
#endif

#define BUFSIZE (4096)


ControllerServer::ControllerServer(int max_chunks, int chunk_size,
                                   int ts_min_rate, int ts_max_rate,
                                   TorPTServer *pt, SocksProxyServer *sp,
                                   CliUnixServer *cli, TrafficShaper *ts)
    : _max_chunks(max_chunks), _chunk_size(chunk_size), _ts_min_rate(ts_min_rate),
      _ts_max_rate(ts_max_rate), _frame_pool(20, max_chunks, chunk_size),
      _chaff_frame(1, chunk_size)
{
    assert(pt != NULL && sp != NULL && cli != NULL && ts != NULL);
    _pt = pt;
    _sp = sp;
    _cli = cli;
    _ts = ts;

    _chaff_frame.setFrameType(FRAME_TYPE_CHAFF);
    int status = _chaff_frame.setChaffFrameData();
    assert(status == FRAME_OK);
}


void ControllerServer::handleSocksNewConnection(FdPair *fdp)
{
    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Handling new connection (client: %d, local: %d)",
                fdp->get_fd0(), fdp->get_fd1());
    #endif

    _client_manager.add_client(fdp);

    ts_rate_update();

    //wake up traffic shaper, there are at least one client
    if (_client_manager.size() == 1) {
        _ts->on();
    }
}

void ControllerServer::handleSocksClientDataReady(FdPair *fdp)
{
    int nread, nwrite, status;
    Frame *frame;
    char *din_ptr; int din_sz, chunk, total_chunks = 1;
    bool ssl_try = false;
    FrameControlFields fcf;

    #if DATA_FRAMES_SYNC_DLV
        FrameQueue *reception_queue;
    #else
        char *dout_ptr; int dout_sz;
    #endif

    #if USE_SSL
        chunk = _client_manager.getTmpChunk(fdp);

        if (chunk == -1) {
            if (_frame_pool.allocFrame(frame) == FRAME_POOL_ERR_FULL) {
                #if (LOG_VERBOSE & LOG_BIT_CONN)
                    _sp->log("ClientDataReady: Frame Pool Full!");
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

    //read all chunks from the frame
    //when reading the first chunk, get the number of chunks
    for (; chunk < total_chunks; chunk++) {
        frame->probeChunk(chunk, din_ptr, din_sz);

        nread = _sp->readn_msg_client(fdp, din_ptr, din_sz);
        if (nread <= 0) {
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
        case FRAME_TYPE_CHAFF:
            _client_manager.setReceptionMark(fdp);

            #if STATS
                _stats.add_no_data_bytes_rec(nread);
            #endif

        break;

        case FRAME_TYPE_DATA:

        #if STATS
            status = frame->getDataFrameData(din_ptr, din_sz);
            assert(status == FRAME_OK);
            _stats.add_bytes_rec(din_sz);
        #endif

        #if SYNC_DLV_STATS
            _dlv_stats.updateDataFrames();
        #endif

        #if DATA_FRAMES_SYNC_DLV
            reception_queue = _client_manager.getReceptionQueue(fdp);
            reception_queue->push(frame);

        #else
            status = frame->getDataFrameData(dout_ptr, dout_sz);
            assert(status == FRAME_OK);
            nwrite = _sp->writen_msg_local(fdp, dout_ptr, dout_sz);

            if (nwrite <= 0) {
                _sp->shutdown_connection(fdp);

            }
            else {
                assert(nwrite == dout_sz);

                #if (LOG_VERBOSE & LOG_BIT_CONN)
                _sp->log("Read from client bytes (%d), wrote (%d), actually (%d)",
                        nread, dout_sz, nwrite);
                #endif

                #if STATS
                    _stats.add_tor_bytes_sent(dout_sz);
                #endif
            }

        #endif
        break;

        case FRAME_TYPE_CTRL:
            _client_manager.setReceptionMark(fdp);

            #if STATS
                _stats.add_no_data_bytes_rec(nread);
            #endif

            status = frame->getCtrlFrameData(fcf);
            assert(status == FRAME_OK);

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

        break;

        default:
            #if (LOG_VERBOSE & LOG_BIT_CONN)
                _sp->log("Wrong frame type, dropping frame. %d", frame->getFrameType());
            #endif
            status = _frame_pool.unallocFrame(frame);
            assert(status == FRAME_OK);
        return;
    }

    /* Check for pending data messages, now that I've received one frame */
    #if DATA_FRAMES_SYNC_DLV
        bool have_recpt_frames = _client_manager.handleReceptionFrames([this]
                                (FdPair *fdp, Client *client) {

            int status, nwrite;
            Frame *frame;
            char *dout_ptr; int dout_sz;

            status = client->getReceivedFrame(frame);
            assert(status != RECP_NO_FRAME_AVAIL);

            if (status == RECP_DATA_FRAME) {

                status = frame->getDataFrameData(dout_ptr, dout_sz);
                assert(status == FRAME_OK);

                //only delivery client frame to Tor if restriction holds
                //otherwise drop frame since client is about to be informed
                if (client->getState() == CLIENT_STATE_ACTIVE) {
                    nwrite = _sp->writen_msg_local(fdp, dout_ptr, dout_sz);

                    #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                        _sp->log("Delivered DATA frame to client %d", fdp->get_fd0());
                    #endif

                    if (nwrite <= 0) {
                        _sp->shutdown_connection(fdp);
                    }
                    else {
                        assert(nwrite == dout_sz);

                        #if (LOG_VERBOSE & LOG_BIT_CONN)
                        _sp->log("Wrote (%d) to client (%d), actually (%d)",
                                dout_sz, fdp->get_fd0(), nwrite);
                        #endif

                        #if STATS
                            _stats.add_tor_bytes_sent(dout_sz);
                        #endif
                    }

                }
                #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
                else {
                    _sp->log("Dropped frame from client %d since client is %d!",
                            fdp->get_fd0(), client->getState());
                }
                #endif

                status = _frame_pool.unallocFrame(frame);
                assert(status == FRAME_OK);
            }

            client->clearReceivedFrame();
        });

        #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
            if (frame->getFrameType() == FRAME_TYPE_DATA && !have_recpt_frames) {
                _sp->log("Delaying delivering DATA frames since I do not \
receive a frame from at least one client.");
            }
        #endif

        #if SYNC_DLV_STATS
            if (frame->getFrameType() == FRAME_TYPE_DATA && !have_recpt_frames) {
                _dlv_stats.updateRetainedFrames();
            }
        #endif

        if (frame->getFrameType() != FRAME_TYPE_DATA) {
            status = _frame_pool.unallocFrame(frame);
            assert(status == FRAME_OK);
        }

    #else
        status = _frame_pool.unallocFrame(frame);
        assert(status == FRAME_OK);

    #endif
}


void ControllerServer::handleSocksBridgeDataReady(FdPair *fdp)
{
    int nread, stat = 0;
    char *din_ptr; int space_sz;
    Frame *frame;

    if (_frame_pool.allocFrame(frame) == FRAME_POOL_ERR_FULL) {
        #if (LOG_VERBOSE & LOG_BIT_CONN)
            _sp->log("ClientDataReady: Frame Pool Full!");
        #endif
        return;
    }

    frame->setFrameType(FRAME_TYPE_DATA);
    stat += frame->getDataFrameSpace(din_ptr, space_sz);

    nread = _sp->read_msg_local(fdp, din_ptr, space_sz);
    if (nread <= 0) {
        _frame_pool.unallocFrame(frame);
        #if (LOG_VERBOSE & LOG_BIT_CONN)
            _sp->log("Local closed! %d err: %d", nread, errno);
        #endif
        // shutdown Tor connection if the user doesn't send any data, but keep
        // the TorK channel open
        _sp->shutdown_local_connection(fdp);
        return;
    }
    stat += frame->setDataFrameSize(nread);
    assert(stat == FRAME_OK);

    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Read from local num bytes (%d)", nread);
    #endif

    #if STATS
        _stats.add_tor_bytes_rec(nread);
    #endif

    //if (_client_manager.getClientState(fdp) != CLIENT_STATE_INACTIVE) {
        FrameQueue* queue = _client_manager.getDataQueue(fdp);
        queue->push(frame);
    /*}
    else {
        _frame_pool.unallocFrame(frame);

        #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
            _sp->log("Dropped frame from local to client %d since client IS NOT ACTIVE!",
                fdp->get_fd0());
        #endif
    }*/
}


void ControllerServer::handleSocksConnectionTerminated(FdPair *fdp)
{
    #if (LOG_VERBOSE & LOG_BIT_CONN)
        _sp->log("Terminated connection (client: %d, bridge: %d)",
                fdp->get_fd0(), fdp->get_fd1());
    #endif

    _client_manager.remove_client(fdp, &_frame_pool);

    //idle traffic shaper, no clients are connected no need to run handler
    if (_client_manager.empty()) {
        _ts->idle();
    } else {
        //order WAIT for active clients whose restriction is broken
        order_WAIT();

        //order CHANGE for active clients
        order_CHANGE();

        ts_rate_update();
    }
}


void ControllerServer::handleCliRequest(std::string &request, std::string &response)
{
    std::string cmd;
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
        std::cerr << "Cli request: " << request << "." << std::endl;
    #endif

    if (cmd == "stats_fp") {
        response = (boost::format("%d\t%d\t%d\n")
                % _frame_pool.getNumAllocFrames()
                % _frame_pool.getNumUnallocFrames()
                % _frame_pool.size()).str();

    } else if (cmd == "stats_clients") {
        response = (boost::format("%d / %d\t%d\t%d\t%d\n")
                % _client_manager.getNumberClients()
                % _client_manager.size()
                % _client_manager.getTotalDataFrames()
                % _client_manager.getTotalCtrlFrames()
                % _client_manager.getTotalRecpFrames()).str();
    } else if (cmd == "ts") {
        response = (boost::format("%d\t%d\n")
                    % _ts->getRate()
                    % _ts->getState()).str();
    } else if (cmd == "ts_rate") {

        if (params.size() != 2) {
            response = "Invalid value\nUsage: ts_rate <TS RATE>\n";
        } else {
            order_TS_RATE(std::stoi(params[1]));

            response = "OK\n";
        }

    }
    #if STATS
        else if (cmd == "stats_bytes") {
            response = (boost::format("%ld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n")
                    % time(NULL)
                    % _stats.getCBytesReceived()
                    % _stats.getCBytesSent()
                    % _stats.getCTorBytesReceived()
                    % _stats.getCTorBytesSent()
                    % _stats.getCNoDataReceived()
                    % _stats.getCNoDataSent()
                    % _client_manager.getNumberClients()
                    % _client_manager.size()).str();
        } else if (cmd == "stats_frames") {
            _client_manager.safeIterate([&response](FdPair *fdp, Client *client) {
                response += (boost::format("%ld\t%d\t%d\t%d\t%d\t%d\n")
                    % time(NULL)
                    % fdp->get_fd0()
                    % client->getTotalCtrlFrames()
                    % client->getTotalDataFrames()
                    % client->getTotalReceptionFrames()
                    % client->getReceptionMark()).str();
            });
        }
    #endif
    #if TIME_STATS
        else if (cmd == "stats_time") {
            response = "";
            _time_stats.getResults(response);
        } else if (cmd == "stats_time_clear") {
            _time_stats.clear();
            response = "Time Stats Cleared!\n";
        }
    #endif
    #if SYNC_DLV_STATS
        else if (cmd == "sync_dlv") {
            response = (boost::format("%d\n")
                    % _dlv_stats.getRetainedFrames()).str();
        } else if (cmd == "clear_dlv") {
            _dlv_stats.clearRetainedFrames();
            _dlv_stats.clearDataFrames();
            response = "OK\n";
        }
    #endif

    else if (cmd == "json") {
        std::string ctrlTimes = "";
        std::string dataTimes = "";
        std::string chaffTimes = "";

        #if TIME_STATS
            _time_stats.getCtrlResults(ctrlTimes);
            _time_stats.getDataResults(dataTimes);
            _time_stats.getChaffResults(chaffTimes);
            _time_stats.clear();

            response = (boost::format("{\"fp\":[{\"f_alloc\": %d },"
            "{\"f_unalloc\": %d}, {\"f_total\": %d}],"
            "\"clients\": %d, \"dataFrames\": %d, \"ctrlFrames\": %d, \"recpFrames\": %d,"
            "\"stats_time\": {\"ctrlTimes\": [%s], \"dataTimes\": [%s], \"chaffTimes\": [%s]}}\n")
            % _frame_pool.getNumAllocFrames()
            % _frame_pool.getNumUnallocFrames()
            % _frame_pool.size()
            % _client_manager.getNumberClients()
            % _client_manager.getTotalDataFrames()
            % _client_manager.getTotalCtrlFrames()
            % _client_manager.getTotalRecpFrames()
            % ctrlTimes
            % dataTimes
            % chaffTimes).str();
        #else
            #if SYNC_DLV_STATS
                response = (boost::format("{\"fp\":[{\"f_alloc\": %d }, "
                "{\"f_unalloc\": %d}, {\"f_total\": %d}], "
                "\"clients\": %d, \"dataFrames\": %d, \"ctrlFrames\": %d, \"recpFrames\": %d, "
                "\"ts\": [{\"rate\": %d, \"state\": %d}], \"retained_frames_dlv\": %d, \"data_frames_dlv\": %d}\n")
                % _frame_pool.getNumAllocFrames()
                % _frame_pool.getNumUnallocFrames()
                % _frame_pool.size()
                % _client_manager.getNumberClients()
                % _client_manager.getTotalDataFrames()
                % _client_manager.getTotalCtrlFrames()
                % _client_manager.getTotalRecpFrames()
                % _ts->getRate()
                % _ts->getState()
                % _dlv_stats.getRetainedFrames()
                % _dlv_stats.getDataFrames()).str();
            #else
                response = (boost::format("{\"fp\":[{\"f_alloc\": %d },"
                "{\"f_unalloc\": %d}, {\"f_total\": %d}],"
                "\"clients\": %d, \"dataFrames\": %d, \"ctrlFrames\": %d, \"recpFrames\": %d, "
                "\"ts\": [{\"rate\": %d, \"state\": %d}]}\n")
                % _frame_pool.getNumAllocFrames()
                % _frame_pool.getNumUnallocFrames()
                % _frame_pool.size()
                % _client_manager.getNumberClients()
                % _client_manager.getTotalDataFrames()
                % _client_manager.getTotalCtrlFrames()
                % _client_manager.getTotalRecpFrames()
                % _ts->getRate()
                % _ts->getState()).str();
            #endif
        #endif
    }
    else {
        response = "NOK\n";
    }
}

void ControllerServer::handleTrafficShapingEvent()
{
    _client_manager.safeIterate([this](FdPair* fdp, Client* client) {
        #if TIME_STATS
            auto start = std::chrono::high_resolution_clock::now();
            int type;
        #endif

        FrameQueue* ctrl_frame_queue = client->getCtrlQueue();
        FrameQueue* data_frame_queue = client->getDataQueue();
        Frame* frame_to_send;
        int status, nwrite, chunk_sz, chunk;
        char* chunk_ptr;

        /* does last SSL write returned SSL_WANT_WRITE?
        If yes, resume frame type. */
        int ssl_partial_frame = client->getWRTmpFrameType();

        bool partial_data_frame = !data_frame_queue->empty() && data_frame_queue->getLastChunk() > 0;

        //Control frames have priority unless we already sent chunks from a data frame
        if ((ssl_partial_frame == -1 || ssl_partial_frame == FRAME_TYPE_CTRL) &&
            (!partial_data_frame && !ctrl_frame_queue->empty())) {
            frame_to_send = ctrl_frame_queue->getFrame();

            status = frame_to_send->probeChunk(0, chunk_ptr, chunk_sz);
            assert(status == FRAME_OK);

            nwrite = _sp->writen_msg_client(fdp, chunk_ptr, chunk_sz);

            if (nwrite != SSL_TRY_LATER) {
                ctrl_frame_queue->pop();
                _frame_pool.unallocFrame(frame_to_send);
                #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                    _sp->log("Popped from ctrl queue. Left %d ", ctrl_frame_queue->size());
                #endif
            } else {
                client->setWRTmpFrameType(FRAME_TYPE_CTRL);
            }

            #if STATS
                if (nwrite > 0) {
                    _stats.add_no_data_bytes_sent(nwrite);
                }
            #endif
            #if TIME_STATS
                type = FRAME_TYPE_CTRL;
            #endif

            //No control frames pending for this client, check for data frames
        } else if ((ssl_partial_frame == -1 || ssl_partial_frame == FRAME_TYPE_DATA) &&
                    !data_frame_queue->empty()) {
            frame_to_send = data_frame_queue->getFrame();
            chunk = data_frame_queue->getLastChunk();

            #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                _sp->log("Got frame from queue.");
            #endif

            status = frame_to_send->probeChunk(chunk, chunk_ptr, chunk_sz);
            assert(status == FRAME_OK);

            nwrite = _sp->writen_msg_client(fdp, chunk_ptr, chunk_sz);

            if (nwrite != SSL_TRY_LATER) {
                if (chunk + 1 < frame_to_send->getNumChunks()) {
                    data_frame_queue->setLastChunk(chunk + 1);
                } else {
                    #if STATS
                        if (nwrite > 0) {
                            char* data_ptr; int data_sz;
                            status = frame_to_send->getDataFrameData(data_ptr, data_sz);
                            assert(status == FRAME_OK);
                            _stats.add_bytes_sent(data_sz);
                        }
                    #endif

                    data_frame_queue->pop();
                    _frame_pool.unallocFrame(frame_to_send);
                    #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                        _sp->log("Popped from data queue. Left %d ", data_frame_queue->size());
                    #endif

                }
            } else {
                client->setWRTmpFrameType(FRAME_TYPE_DATA);
            }

            #if TIME_STATS
                type = FRAME_TYPE_DATA;
            #endif

        } else { // Nor control frames nor data frames available, send chaff instead
            frame_to_send = &_chaff_frame;

            status = frame_to_send->probeChunk(0, chunk_ptr, chunk_sz);
            assert(status == FRAME_OK);

            nwrite = _sp->writen_msg_client(fdp, chunk_ptr, chunk_sz);

            if (nwrite == SSL_TRY_LATER) {
                client->setWRTmpFrameType(FRAME_TYPE_CHAFF);
            }

            #if STATS
                if (nwrite > 0) {
                    _stats.add_no_data_bytes_sent(nwrite);
                }
            #endif

            #if TIME_STATS
                type = FRAME_TYPE_CHAFF;
            #endif
        }



        if (nwrite <= 0) {
            #if (LOG_VERBOSE & LOG_BIT_TR_SHAPER)
                _sp->log("Failed to send! Error %d", nwrite);
            #endif
        }
        else {
            assert(nwrite == chunk_sz);

            if (ssl_partial_frame != -1) {
                client->setWRTmpFrameType(-1);
            }

            #if TIME_STATS
                auto stop = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
                switch (type) {
                    case FRAME_TYPE_CTRL: _time_stats.addCtrlTime(duration.count()); break;
                    case FRAME_TYPE_DATA: _time_stats.addDataTime(duration.count()); break;
                    case FRAME_TYPE_CHAFF: _time_stats.addChaffTime(duration.count());
                }
            #endif
        }

    });
}

/* ======================= CTRL Frames Handlers ======================= */

void ControllerServer::handleCtrlFrame_NULL(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_NULL))
        _sp->log("Received NULL frame from client. Ignoring.");
    #endif
}

void ControllerServer::handleCtrlFrame_HELLO(FdPair *fdp, FrameControlFields &fcf) {
    Frame *reply;
    FrameControlFields reply_fcf;
    int status;

    status = _frame_pool.allocFrame(reply);
    assert(status == FRAME_OK);
    reply->setFrameType(FRAME_TYPE_CTRL);

    status = _client_manager.updateClientKMin(fdp, fcf._k_min);
    _client_manager.updateClientState(fdp, CLIENT_STATE_CONNECTED);

    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO))
        if (status == CLIENT_MANAGER_OK) {
            _sp->log("Registed k-min = %d of client %d", fcf._k_min,
                                                        fdp->get_fd0());
        } else if (status == CLIENT_MANAGER_ERR_INVALID) {
            _sp->log("Invalid k-min = %d of client %d", fcf._k_min,
                                                        fdp->get_fd0());
        }
    #endif

    reply_fcf._type = FRAME_CTRL_TYPE_HELLO_OK;
    status = reply->setCtrlFrameData(&reply_fcf);
    assert(status == FRAME_OK);

    _client_manager.getCtrlQueue(fdp)->push(reply);

    Frame* ctrl_frame;
    FrameControlFields ctrl_fcf;

    ctrl_fcf._type = FRAME_CTRL_TYPE_ACTIVE;

    //order ACTIVE for wating clients whose restriction is now fulfilled
    for (FdPair *fulfilled_client : _client_manager.getWaitingFulfilledClients()) {
        status = _frame_pool.allocFrame(ctrl_frame);
        assert(status == FRAME_OK);

        ctrl_frame->setFrameType(FRAME_TYPE_CTRL);

        status = ctrl_frame->setCtrlFrameData(&ctrl_fcf);
        assert(status == FRAME_OK);

        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO))
            _sp->log("Ordering ACTIVE to fulfilled client %d!",
                        fulfilled_client->get_fd0());
        #endif

        _sp->restore_local_connection(fulfilled_client);
        _client_manager.updateClientState(fulfilled_client, CLIENT_STATE_ACTIVE);
        _client_manager.getCtrlQueue(fulfilled_client)->push(ctrl_frame);
    }
}

void ControllerServer::handleCtrlFrame_HELLO_OK(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO_OK))
        _sp->log("Received HELLO_OK from client. Ignoring.");
    #endif
}

void ControllerServer::handleCtrlFrame_ACTIVE(FdPair *fdp) {
    Frame *reply;
    FrameControlFields reply_fcf;
    int status, state;

    status = _frame_pool.allocFrame(reply);
    assert(status == FRAME_OK);
    reply->setFrameType(FRAME_TYPE_CTRL);

    state = _client_manager.getClientState(fdp);

    if (state != CLIENT_STATE_CONNECTED &&
        state != CLIENT_STATE_INACTIVE) {

        reply_fcf._type = FRAME_CTRL_TYPE_ERR_ACTIVE;

    } else {

        if (!_client_manager.isClientBroken(fdp)) {
            //order CHANGE for active clients
            order_CHANGE();

            _sp->restore_local_connection(fdp);
            reply_fcf._type = FRAME_CTRL_TYPE_ACTIVE;
            _client_manager.updateClientState(fdp, CLIENT_STATE_ACTIVE);


        } else {
            reply_fcf._type = FRAME_CTRL_TYPE_WAIT;
            _client_manager.updateClientState(fdp, CLIENT_STATE_WAIT);
            status = _sp->shutdown_local_connection(fdp);
            assert(status == 0);
        }

    }

    status = reply->setCtrlFrameData(&reply_fcf);
    assert(status == FRAME_OK);

    _client_manager.getCtrlQueue(fdp)->push(reply);
}

void ControllerServer::handleCtrlFrame_WAIT(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_WAIT))
        _sp->log("Received CTRL WAIT frame from client %d, but clients \
aren't supposed to send them. Ignored.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_CHANGE(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE))
        _sp->log("Received CTRL CHANGE frame from client %d, but clients \
aren't supposed to send them. Ignored.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_CHANGE_OK(FdPair *fdp) {
    int state;

    state = _client_manager.getClientState(fdp);

    if (state == CLIENT_STATE_CHANGING) {
        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE_OK))
            _sp->log("Received CHANGE OK from client %d.", fdp->get_fd0());
        #endif
        _sp->restore_local_connection(fdp);
        _client_manager.updateClientState(fdp, CLIENT_STATE_ACTIVE);

    }
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_CHANGE_OK))
    else if (state == CLIENT_STATE_UNDEF) {
        _sp->log("Received CHANGE OK from UNDEF client %d.", fdp->get_fd0());
    }
    else if (state == CLIENT_STATE_HELLO) {
        _sp->log("Received CHANGE OK from HELLO client %d.", fdp->get_fd0());
    }
    else if (state == CLIENT_STATE_CONNECTED) {
        _sp->log("Received CHANGE OK from CONNECTED client %d.", fdp->get_fd0());
    }
    else if (state == CLIENT_STATE_ACTIVE) {
        _sp->log("Received CHANGE OK from ACTIVE client %d.", fdp->get_fd0());
    }
    else if (state == CLIENT_STATE_WAIT) {
        _sp->log("Received CHANGE OK from WAIT client %d.", fdp->get_fd0());
    }
    else if (state == CLIENT_STATE_INACTIVE) {
        _sp->log("Received CHANGE OK from INACTIVE client %d.", fdp->get_fd0());
    }
    #endif

}

void ControllerServer::handleCtrlFrame_INACTIVE(FdPair *fdp) {
    Frame *reply;
    FrameControlFields reply_fcf;
    int status, state;

    status = _frame_pool.allocFrame(reply);
    assert(status == FRAME_OK);
    reply->setFrameType(FRAME_TYPE_CTRL);

    state = _client_manager.getClientState(fdp);

    if (state != CLIENT_STATE_ACTIVE && state != CLIENT_STATE_WAIT) {
        reply_fcf._type = FRAME_CTRL_TYPE_ERR_INACTIVE;

    } else {
        reply_fcf._type = FRAME_CTRL_TYPE_INACTIVE;
        _client_manager.updateClientState(fdp, CLIENT_STATE_INACTIVE);
        status = _sp->shutdown_local_connection(fdp);
        assert(status == 0);
    }

    status = reply->setCtrlFrameData(&reply_fcf);
    assert(status == FRAME_OK);

    _client_manager.getCtrlQueue(fdp)->push(reply);
}

void ControllerServer::handleCtrlFrame_SHUT(FdPair *fdp) {
    Frame *reply;
    FrameControlFields reply_fcf;
    int status;

    status = _frame_pool.allocFrame(reply);
    assert(status == FRAME_OK);
    reply->setFrameType(FRAME_TYPE_CTRL);

    reply_fcf._type = FRAME_CTRL_TYPE_SHUT_OK;
    _client_manager.updateClientState(fdp, CLIENT_STATE_SHUT);

    status = reply->setCtrlFrameData(&reply_fcf);
    assert(status == FRAME_OK);

    //order WAIT for active clients whose restriction is broken
    order_WAIT();

    //order CHANGE for active clients
    order_CHANGE();

    _client_manager.getCtrlQueue(fdp)->push(reply);
}

void ControllerServer::handleCtrlFrame_SHUT_OK(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_SHUT_OK))
        _sp->log("Received CTRL SHUT OK frame from client %d, but \
clients aren't supposed to send them. Ignored.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_TS_RATE(FdPair *fdp, FrameControlFields &fcf) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_TS_RATE))
        _sp->log("Received CTRL TS RATE frame from client %d, but \
clients aren't supposed to send them. Ignored.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_ERR_HELLO(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_HELLO))
        _sp->log("Received ERR HELLO frame from client %d.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_ERR_ACTIVE(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_ACTIVE))
        _sp->log("Received ERR ACTIVE frame from client %d.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_ERR_INACTIVE(FdPair *fdp) {
    #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
        (LOG_CTRL_TYPES & LOG_BIT_TYPE_INACTIVE))
        _sp->log("Received ERR INACTIVE frame from client %d.", fdp->get_fd0());
    #endif
}

void ControllerServer::handleCtrlFrame_UNKNOWN(FdPair *fdp) {
    #if (LOG_VERBOSE & LOG_BIT_CTRL_FRAMES)
        _sp->log("Received UNKNOWN frame from client %d. Ignored.", fdp->get_fd0());
    #endif
}

/* ==================== CTRL Frames Creation Helpers ==================== */

void ControllerServer::order_CHANGE() {
    Frame *change_frame;
    FrameControlFields change_fcf;
    int status;

    for (FdPair *client : _client_manager.getFulfilledClients()) {
        status = _frame_pool.allocFrame(change_frame);
        assert(status == FRAME_OK);

        change_frame->setFrameType(FRAME_TYPE_CTRL);
        change_fcf._type = FRAME_CTRL_TYPE_CHANGE;

        status = change_frame->setCtrlFrameData(&change_fcf);
        assert(status == FRAME_OK);

        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_CTRL_TYPE_CHANGE))
            _sp->log("Ordering CHANGE to client %d!",
                    client->get_fd0());
        #endif

        _client_manager.updateClientState(client, CLIENT_STATE_CHANGING);

        status = _sp->shutdown_local_connection(client);
        assert(status == 0);

        _client_manager.getCtrlQueue(client)->push(change_frame);
    }
}

void ControllerServer::order_WAIT() {
    Frame *ctrl_frame;
    FrameControlFields fcf;
    int status;

    for (FdPair *broken_client : _client_manager.getBrokenClients()) {
        status = _frame_pool.allocFrame(ctrl_frame);
        assert(status == FRAME_OK);

        ctrl_frame->setFrameType(FRAME_TYPE_CTRL);
        fcf._type = FRAME_CTRL_TYPE_WAIT;

        status = ctrl_frame->setCtrlFrameData(&fcf);
        assert(status == FRAME_OK);

        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_WAIT))
            _sp->log("Ordering WAIT to broken client %d!",
                    broken_client->get_fd0());
        #endif

        _client_manager.updateClientState(broken_client, CLIENT_STATE_WAIT);

        status = _sp->shutdown_local_connection(broken_client);
        assert(status == 0);

        _client_manager.getCtrlQueue(broken_client)->push(ctrl_frame);
    }
}

void ControllerServer::order_TS_RATE(unsigned int rate) {

    assert(rate >= _ts_min_rate && rate <= _ts_max_rate);

    _client_manager.safeIterate([this, rate](FdPair *fdp, Client *client) {
        Frame *ctrl_frame;
        FrameControlFields fcf;
        int status;

        status = _frame_pool.allocFrame(ctrl_frame);
        assert(status == FRAME_OK);

        ctrl_frame->setFrameType(FRAME_TYPE_CTRL);
        fcf._type = FRAME_CTRL_TYPE_TS_RATE;
        fcf._ts_rate = rate;

        status = ctrl_frame->setCtrlFrameData(&fcf);
        assert(status == FRAME_OK);

        #if ((LOG_VERBOSE & LOG_BIT_CTRL_FRAMES) && \
            (LOG_CTRL_TYPES & LOG_BIT_TYPE_TS_RATE))
            _sp->log("Ordering TS_RATE of %d microsec to client %d!",
                    rate, fdp->get_fd0());
        #endif

        client->getCtrlQueue()->push(ctrl_frame);
    });

}

void ControllerServer::ts_rate_update() {
    unsigned int number_of_users = _client_manager.size();
    unsigned int rate = (number_of_users <= MAX_LISTEN_USERS) ?
                                    number_of_users * _ts_min_rate
                                    : _ts_max_rate;
    //send ctrl frame for clients change their TS rate
    order_TS_RATE(rate);

    //change bridge TS rate
    _ts->setRate(rate);
}
