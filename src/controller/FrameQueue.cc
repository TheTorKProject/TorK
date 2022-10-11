#include "FrameQueue.hh"

FrameQueue::FrameQueue() : _last_chunk(0) {}

void FrameQueue::push(Frame* frame) {
    std::unique_lock<std::shared_mutex> res_lock(_mtx);

    _queue.push(frame);
}

void FrameQueue::pop() {
    std::unique_lock<std::shared_mutex> res_lock(_mtx);

    _last_chunk = 0;
    _queue.pop();
}

Frame* FrameQueue::getFrame() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);

    return _queue.front();
}

bool FrameQueue::empty() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);

    return _queue.empty();
}

int FrameQueue::size() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);

    return _queue.size();
}

int FrameQueue::getLastChunk() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);

    return _last_chunk;
}

void FrameQueue::setLastChunk(int chunk) {
    std::unique_lock<std::shared_mutex> res_lock(_mtx);

    _last_chunk = chunk;
}