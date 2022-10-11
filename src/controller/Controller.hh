#ifndef CONTROLLER_HH
#define CONTROLLER_HH

#include "FdPair.hh"
#include "Frame.hh"

class Controller {
    public:
        virtual void handleSocksNewConnection(FdPair *fds)        = 0;
        virtual void handleSocksClientDataReady(FdPair *fds)      = 0;
        virtual void handleSocksBridgeDataReady(FdPair *fds)      = 0;
        virtual void handleSocksConnectionTerminated(FdPair *fds) = 0;
        virtual void handleTrafficShapingEvent()                  = 0;
        virtual void handleCliRequest(std::string &request,
                                      std::string &response) = 0;

        virtual void handleCtrlFrame_NULL     (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_HELLO    (FdPair *fdp,
                                               FrameControlFields &fcf)   = 0;
        virtual void handleCtrlFrame_HELLO_OK (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_ACTIVE   (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_WAIT     (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_CHANGE   (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_CHANGE_OK(FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_INACTIVE (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_SHUT     (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_SHUT_OK  (FdPair *fdp)   = 0;
        virtual void handleCtrlFrame_TS_RATE  (FdPair *fdp,
                                               FrameControlFields &fcf)   = 0;

        virtual void handleCtrlFrame_ERR_HELLO   (FdPair *fdp) = 0;
        virtual void handleCtrlFrame_ERR_ACTIVE  (FdPair *fdp) = 0;
        virtual void handleCtrlFrame_ERR_INACTIVE(FdPair *fdp) = 0;

        virtual void handleCtrlFrame_UNKNOWN     (FdPair *fdp) = 0;
};

/* Object for storing debug stats values */
#if STATS
class Stats {
    public:
        Stats() : _bytes_received(0), _bytes_sent(0), _tor_bytes_received(0),
                  _tor_bytes_sent(0), _no_data_received(0), _no_data_sent(0) {};

        void add_bytes_rec(int bytes) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _bytes_received += (bytes > 0) ? bytes : 0;
        }

        void add_bytes_sent(int bytes) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _bytes_sent += (bytes > 0) ? bytes : 0;
        }

        void add_tor_bytes_rec(int bytes) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _tor_bytes_received += (bytes > 0) ? bytes : 0;
        }

        void add_tor_bytes_sent(int bytes) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _tor_bytes_sent += (bytes > 0) ? bytes : 0;
        }

        void add_no_data_bytes_rec(int bytes) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _no_data_received += (bytes > 0) ? bytes : 0;
        }

        void add_no_data_bytes_sent(int bytes) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _no_data_sent += (bytes > 0) ? bytes : 0;
        }

        unsigned int getCBytesReceived() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            unsigned int old_val = _bytes_received;
            _bytes_received = 0;
            return old_val;
        }

        unsigned int getCBytesSent() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            unsigned int old_val = _bytes_sent;
            _bytes_sent = 0;
            return old_val;
        }

        unsigned int getCTorBytesReceived() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            unsigned int old_val = _tor_bytes_received;
            _tor_bytes_received = 0;
            return old_val;
        }

        unsigned int getCTorBytesSent() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            unsigned int old_val = _tor_bytes_sent;
            _tor_bytes_sent = 0;
            return old_val;
        }

        unsigned int getCNoDataReceived() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            unsigned int old_val = _no_data_received;
            _no_data_received = 0;
            return old_val;
        }

        unsigned int getCNoDataSent() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            unsigned int old_val = _no_data_sent;
            _no_data_sent = 0;
            return old_val;
        }

    private:
        unsigned int _bytes_received;
        unsigned int _bytes_sent;
        unsigned int _tor_bytes_received;
        unsigned int _tor_bytes_sent;
        unsigned int _no_data_received;
        unsigned int _no_data_sent;

        std::mutex _mtx;
};
#endif

#if TIME_STATS
class TimeStats {
    public:
        TimeStats() {};

        void addCtrlTime(int time) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            if (_handler_ctrl.size() < TIME_STATS) {
                _handler_ctrl.push_back(time);
            }
        }

        void addDataTime(int time) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            if (_handler_data.size() < TIME_STATS) {
                _handler_data.push_back(time);
            }
        }

        void addChaffTime(int time) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            if (_handler_chaff.size() < TIME_STATS) {
                _handler_chaff.push_back(time);
            }
        }

        void getResults(std::string &output) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            output += "CTRL:\n";
            for (int time : _handler_ctrl) {
                output += std::to_string(time) + "\n";
            }
            output += "\nDATA:\n";
            for (int time : _handler_data) {
                output += std::to_string(time) + "\n";
            }
            output += "\nCHAFF:\n";
            for (int time : _handler_chaff) {
                output += std::to_string(time) + "\n";
            }
        }

        void getCtrlResults(std::string &output) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            for (int i = 0; i < _handler_ctrl.size(); i++) {
                if (i < _handler_ctrl.size() - 1)
                    output += std::to_string(_handler_ctrl[i]) + ", ";
                else
                    output += std::to_string(_handler_ctrl[i]);
            }
        }

        void getDataResults(std::string &output) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            for (int i = 0; i < _handler_data.size(); i++) {
                if (i < _handler_data.size() - 1)
                    output += std::to_string(_handler_data[i]) + ", ";
                else
                    output += std::to_string(_handler_data[i]);
            }
        }

        void getChaffResults(std::string &output) {
            std::unique_lock<std::mutex> res_lock(_mtx);
            for (int i = 0; i < _handler_chaff.size(); i++) {
                if (i < _handler_chaff.size() - 1)
                    output += std::to_string(_handler_chaff[i]) + ", ";
                else
                    output += std::to_string(_handler_chaff[i]);
            }
        }

        void clear() {
            std::unique_lock<std::mutex> res_lock(_mtx);
            _handler_ctrl.clear();
            _handler_data.clear();
            _handler_chaff.clear();
        }

    private:
        std::vector<int> _handler_ctrl;
        std::vector<int> _handler_data;
        std::vector<int> _handler_chaff;

        std::mutex _mtx;
};
#endif

#if SYNC_DLV_STATS
    class SyncDLVStats {
        public:
            SyncDLVStats() : _retained_frames(0) {};

            void updateRetainedFrames() {
                std::unique_lock<std::mutex> res_lock(_mtx);
                _retained_frames++;
            }

            void clearRetainedFrames() {
                std::unique_lock<std::mutex> res_lock(_mtx);
                _retained_frames = 0;
            }

            int getRetainedFrames() {
                std::unique_lock<std::mutex> res_lock(_mtx);
                return _retained_frames;
            }

            void updateDataFrames() {
                std::unique_lock<std::mutex> res_lock(_mtx);
                _data_frames++;
            }

            void clearDataFrames() {
                std::unique_lock<std::mutex> res_lock(_mtx);
                _data_frames = 0;
            }

            int getDataFrames() {
                std::unique_lock<std::mutex> res_lock(_mtx);
                return _data_frames;
            }

        private:
            unsigned int _retained_frames;
            unsigned int _data_frames;

            std::mutex _mtx;
    };
#endif

#if DEBUG_TOOLS

#define DT_DROP_OFF   (0)
#define DT_DROP_ALL   ((1u<<4) - 1)
#define DT_DROP_CTRL  (1u)
#define DT_DROP_DATA  (1u<<1)
#define DT_DROP_CHAFF (1u<<2)
#define DT_DROP_OTHER (1u<<3)

#define DT_CONTROL(F, type, db_info, size) ((db_info.getDropType() & type) ? (size) : (F))

class DebugInfo {
    public:
        DebugInfo() : _drop_type(DT_DROP_OFF) {}

        void setDropType(unsigned int drop_type) {
            std::unique_lock<std::shared_mutex> res_lock(_mtx);
            _drop_type = drop_type;
        }

        unsigned int getDropType() {
            std::shared_lock<std::shared_mutex> res_lock(_mtx);
            return _drop_type;
        }

    private:
        unsigned int _drop_type;

        std::shared_mutex _mtx;
};
#endif

#endif /* CONTROLLER_HH */
