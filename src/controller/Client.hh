#ifndef CLIENT_HH
#define CLIENT_HH

#include "FrameQueue.hh"

#define CLIENT_STATE_UNDEF     (0)
#define CLIENT_STATE_HELLO     (1)
#define CLIENT_STATE_CONNECTED (2)
#define CLIENT_STATE_ACTIVE    (3)
#define CLIENT_STATE_WAIT      (4)
#define CLIENT_STATE_CHANGING  (5)
#define CLIENT_STATE_INACTIVE  (6)
#define CLIENT_STATE_SHUT      (7)
#define CLIENT_STATE_NOT_CONN  (-1)

#define CLIENT_K_MIN_UNDEF    (-1)

#define RECP_DATA_FRAME        (0)
#define RECP_NON_DATA_FRAME    (1)
#define RECP_NO_FRAME_AVAIL    (-1)

class Client {

    public:
        Client() : _state(CLIENT_STATE_UNDEF), _k_min(CLIENT_K_MIN_UNDEF),
                   _reception_mark(false), _tmp_frame(nullptr), _tmp_chunk(-1),
                   _wr_tmp_frame_type(-1) {};

        ~Client() {};

        FrameQueue* getDataQueue() {
            return &_data_queue;
        }

        FrameQueue* getCtrlQueue() {
            return &_ctrl_queue;
        }

        FrameQueue* getReceptionQueue() {
            return &_reception_queue;
        }

        int getState() {
            std::shared_lock<std::shared_mutex> res_lock(_mtx);
            return _state;
        }

        int getKMin() {
            return _k_min;
        }

        void setState(int new_state) {
            std::unique_lock<std::shared_mutex> res_lock(_mtx);
            _state = new_state;
        }

        void setKMin(int k_min) {
            _k_min = k_min;
        }

        void setReceptionMark() {
            _reception_mark = true;
        }

        bool getReceptionMark() {
            return _reception_mark;
        }

        int getTotalDataFrames() {
            return _data_queue.size();
        }

        int getTotalCtrlFrames() {
            return _ctrl_queue.size();
        }

        int getTotalReceptionFrames() {
            return _reception_queue.size();
        }

        bool receivedFrame() {
            return !_reception_queue.empty() || _reception_mark;
        }

        int getReceivedFrame(Frame *(&frame)) {
            if (!_reception_queue.empty()) {
                frame = _reception_queue.getFrame();
                return RECP_DATA_FRAME;

            } else if (_reception_mark) {
                return RECP_NON_DATA_FRAME;

            } else {
                return RECP_NO_FRAME_AVAIL;
            }
        }

        void clearReceivedFrame() {
            if (!_reception_queue.empty()) {
                _reception_queue.pop();

            } else if (_reception_mark) {
                _reception_mark = false;
            }
        }

        Frame* getTmpFrame() {
            return _tmp_frame;
        }

        void setTmpFrame(Frame* tmp_frame) {
            _tmp_frame = tmp_frame;
        }

        int getTmpChunk() {
            return _tmp_chunk;
        }

        void setTmpChunk(int chunk) {
            _tmp_chunk = chunk;
        }

        void setWRTmpFrameType(int frame_type) {
            _wr_tmp_frame_type = frame_type;
        }

        int getWRTmpFrameType() {
            return _wr_tmp_frame_type;
        }


    private:
        FrameQueue _data_queue;
        FrameQueue _ctrl_queue;

        FrameQueue _reception_queue;

        /* store tmp frame and chunk for SSL_TRY_LATER:
         * openssl requires to call SSL_read / SSL_write using the same parameters when
         * returning SSL_WANT_READ / SSL_WANT_WRITE. These parameters save the last frame and chunk
         * used when a SSL_WANT_READ / SSL_WANT_WRITE occurred.
         * */
        Frame* _tmp_frame;
        int _tmp_chunk;
        int _wr_tmp_frame_type;

        int _state;

        int _k_min;

        bool _reception_mark;

        std::shared_mutex _mtx;

};

#endif /* CLIENT_HH */
