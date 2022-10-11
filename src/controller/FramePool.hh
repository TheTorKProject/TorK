#ifndef FRAME_POOL_HH
#define FRAME_POOL_HH

#include "Frame.hh"
#include "FdPair.hh"
#include <queue>
#include <set>

#define FRAME_POOL_MAX_FRAMES   (30000)

#define FRAME_POOL_OK           (0)

#define FRAME_POOL_ERR_FULL               (-1)
#define FRAME_POOL_ERR_INVALID            (-2)

class FramePool {

    public:
        FramePool(int pool_size, int max_chunks, int chunk_size);

        ~FramePool();

        int getNumAllocFrames();

        int getNumUnallocFrames();

        int allocFrame(Frame*(& frame));

        int unallocFrame(Frame*(& frame));

        void printFramePoolInfo(std::ostream &out = std::cout);

        void dumpFramePoolFrames(std::ostream &out = std::cout);

        int size();

    private:
        int _pool_size;
        int _max_chunks;
        int _chunk_size;
        std::queue<Frame*> _unalloc_frames;
        std::set<Frame*> _alloc_frames;

        std::shared_mutex _mtx;

};

#endif /* FRAME_POOL_HH */
