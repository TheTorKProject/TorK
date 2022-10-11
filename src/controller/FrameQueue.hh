#ifndef FRAME_QUEUE_HH
#define FRAME_QUEUE_HH

#include "Frame.hh"
#include <queue>

class FrameQueue {

    public:
        FrameQueue();

        ~FrameQueue() {}

        void push(Frame* frame);

        void pop();

        Frame* getFrame();

        bool empty();

        int size();

        int getLastChunk();

        void setLastChunk(int chunk);

    private:
        std::queue<Frame*> _queue;

        int _last_chunk;

        std::shared_mutex _mtx;

};

#endif /* FRAME_QUEUE_HH */