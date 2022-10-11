#include "FramePool.hh"

FramePool::FramePool(int pool_size, int max_chunks, int chunk_size)
{
    assert(pool_size > 0 && pool_size <= FRAME_POOL_MAX_FRAMES);

    std::unique_lock<std::shared_mutex> res_lock(_mtx);

    _pool_size = pool_size;
    _max_chunks = max_chunks;
    _chunk_size = chunk_size;

    for (int i = 0; i < _pool_size; i++) {
        _unalloc_frames.push(new Frame(_max_chunks, _chunk_size));
    }

    assert(_unalloc_frames.size() == _pool_size);

}

FramePool::~FramePool()
{
    Frame *frame;

    std::unique_lock<std::shared_mutex> res_lock(_mtx);

    while (!_unalloc_frames.empty()) {
        frame = _unalloc_frames.front();
        _unalloc_frames.pop();
        delete frame;
    }

    for (std::set<Frame*>::iterator it = _alloc_frames.begin(); it != _alloc_frames.end(); ++it) {
        delete *it;
    }
}

int FramePool::getNumAllocFrames()
{
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    return _alloc_frames.size();
}

int FramePool::getNumUnallocFrames()
{
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    return _unalloc_frames.size();
}

int FramePool::allocFrame(Frame*(&frame))
{
    std::unique_lock<std::shared_mutex> res_lock(_mtx);

    if (_unalloc_frames.size() == 0) {
        int more_frames;
        if (_pool_size < FRAME_POOL_MAX_FRAMES) {
            if (_pool_size * 2 <= FRAME_POOL_MAX_FRAMES) {
                more_frames = _pool_size;
                _pool_size = _pool_size * 2;

            } else {
                more_frames = FRAME_POOL_MAX_FRAMES - _pool_size;
                _pool_size = FRAME_POOL_MAX_FRAMES;

            }

            for (int i = 0; i < more_frames; i++) {
                _unalloc_frames.push(new Frame(_max_chunks, _chunk_size));
            }
        } else {
            return FRAME_POOL_ERR_FULL;
        }
    }

    frame = _unalloc_frames.front();
    _unalloc_frames.pop();
    _alloc_frames.insert(frame);

    return FRAME_POOL_OK;
}

int FramePool::unallocFrame(Frame*(& frame))
{
    assert(frame != nullptr);

    std::unique_lock<std::shared_mutex> res_lock(_mtx);
    if (_alloc_frames.find(frame) == _alloc_frames.end()) {
        return FRAME_POOL_ERR_INVALID;
    }
    _alloc_frames.erase(frame);
    _unalloc_frames.push(frame);

    return FRAME_POOL_OK;
}

void FramePool::printFramePoolInfo(std::ostream &out)
{
    std::string state;
    std::shared_lock<std::shared_mutex> res_lock(_mtx);

    out << " ************************** FRAME POOL ************************** " << std::endl;
    out << "Alloc Frames " << "("
        << _alloc_frames.size()   << ") " << std::endl;

    for (std::set<Frame*>::iterator it = _alloc_frames.begin(); it != _alloc_frames.end(); ++it) {
        out << " ================ Frame ================ "
            << std::endl;
        (*it)->printFrameInfo(out);
        out << std::endl;
    }

    out << "Unalloc Frames " << "("
        << _unalloc_frames.size()   << ") " << std::endl;
}

void FramePool::dumpFramePoolFrames(std::ostream &out)
{
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    out << " ************************** FRAME POOL ************************** " << std::endl;
    out << "Alloc Frames " << "("
        << _alloc_frames.size()   << ") " << std::endl;

    for (std::set<Frame*>::iterator it = _alloc_frames.begin(); it != _alloc_frames.end(); ++it) {
        out << " ================ Frame ================ "
            << std::endl;
        (*it)->dumpFrame(out);
        out << std::endl;
    }

    out << "Unalloc Frames " << std::endl << "("
        << _unalloc_frames.size()   << ") " << std::endl;
}

int FramePool::size() {
    std::shared_lock<std::shared_mutex> res_lock(_mtx);
    return _pool_size;
}
