#ifndef FRAME_HH
#define FRAME_HH

#include "common/Common.hh"

#define FRAME_OK                    (0)
#define FRAME_ERR_CHUNK_INVALID     (-1)
#define FRAME_ERR_FRAME_FULL        (-2)
#define FRAME_ERR_INSUFF_SPACE      (-3)
#define FRAME_ERR_WRONG_FRAME_TYPE  (-4)
#define FRAME_ERR_DATA_UNAVAILABLE  (-5)
#define FRAME_ERR_WRONG_CTRL_TYPE   (-6)
#define FRAME_ERR_CHUNK_UNALLOC     (-7)
#define FRAME_ERR_WRONG_DATA_SIZE   (-8)

#define FRAME_TYPE_NULL         (0)
#define FRAME_TYPE_CHAFF        (1)
#define FRAME_TYPE_DATA         (2)
#define FRAME_TYPE_CTRL         (3)

#define FRAME_CTRL_TYPE_NULL         (0)
#define FRAME_CTRL_TYPE_HELLO        (1)
#define FRAME_CTRL_TYPE_HELLO_OK     (2)
#define FRAME_CTRL_TYPE_ACTIVE       (3)
#define FRAME_CTRL_TYPE_WAIT         (4)
#define FRAME_CTRL_TYPE_CHANGE       (5)
#define FRAME_CTRL_TYPE_CHANGE_OK    (6)
#define FRAME_CTRL_TYPE_INACTIVE     (7)
#define FRAME_CTRL_TYPE_SHUT         (8)
#define FRAME_CTRL_TYPE_SHUT_OK      (9)
#define FRAME_CTRL_TYPE_TS_RATE      (10)

#define FRAME_CTRL_TYPE_ERR_HELLO    (-1)
#define FRAME_CTRL_TYPE_ERR_ACTIVE   (-2)
#define FRAME_CTRL_TYPE_ERR_INACTIVE (-3)


struct FrameControlFields {
    int _type;
    unsigned int _k_min;
    unsigned int _ts_rate;
};


class Frame {

    public:

        Frame(int max_chunks, int chunk_size);

        ~Frame();

        int getChunkSize();

        int getNumChunks();

        int allocChunk(int how_many);

        int assignChunk(int chunk, char *chunk_ptr, int chunk_sz);

        int probeChunk(int chunk, char *(&chunk_ptr), int &chunk_sz);

        bool compareChunks(char *chunk1_ptr, int chunk1_sz,
                           char *chunk2_ptr, int chunk2_sz);

        int setChunkData(int chunk, char *data_ptr, int data_sz);

        int getChunkData(int chunk, char *(&buff), int &buff_sz);

        void dumpFrame(std::ostream &out = std::cout);

        void printFrameInfo(std::ostream &out = std::cout);

        void setFrameType(int type);

        int getFrameType();

        int setDataFrameData(char *data_ptr, int data_sz);

        int getDataFrameData(char *(&data_ptr), int &data_sz);

        int getDataFrameSpace(char *(&data_ptr), int &space_sz);

        int setDataFrameSize(int data_sz);

        int setChaffFrameData();

        int setCtrlFrameData(FrameControlFields *ctrl);

        int getCtrlFrameData(FrameControlFields &ctrl);

    private:

        char *_buffer;
        int _buffer_size;
        int _max_chunks;
        int _chunk_size;
};

#endif //FRAME_HH
