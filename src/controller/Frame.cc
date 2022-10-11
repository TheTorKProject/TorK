#include <iomanip>
#include <netinet/in.h>

#include "Frame.hh"

#define FRAME_CHUNKS_FIELD      (0)
#define DATA_FRAME_SIZE_FIELD   (2)
#define DATA_FRAME_HEADER_SIZE  ((DATA_FRAME_SIZE_FIELD) + sizeof(unsigned int))
#define FRAME_CTRL_SIZE_HELLO   (sizeof(unsigned int))
#define FRAME_CTRL_SIZE_TS_RATE (sizeof(unsigned int))
#define FRAME_TYPE_FIELD        (1)
#define CTRL_FRAME_TYPE_FIELD   (2)
#define CTRL_FRAME_HEADER_SIZE  ((CTRL_FRAME_TYPE_FIELD) + sizeof(unsigned int))


Frame::Frame(int max_chunks, int chunk_size)
{
    assert(max_chunks > 0 && max_chunks < 256);
    _max_chunks = max_chunks;
    _chunk_size = chunk_size;
    _buffer_size = _max_chunks * _chunk_size;
    _buffer = new char[_buffer_size]();
};


Frame::~Frame() {
    delete[] _buffer;
}


int Frame::getChunkSize()
{
    return _chunk_size;
}


int Frame::getNumChunks()
{
    return (int) _buffer[FRAME_CHUNKS_FIELD];
}


int Frame::allocChunk(int how_many = 1)
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks + how_many > _max_chunks) {
        return FRAME_ERR_FRAME_FULL;
    }

    num_chunks += how_many;
    _buffer[FRAME_CHUNKS_FIELD] = (char) num_chunks;
    return FRAME_OK;
}


int Frame::assignChunk(int chunk, char *chunk_ptr, int chunk_sz)
{
    assert(chunk_ptr != NULL && chunk_sz == _chunk_size);

    if (chunk >= _max_chunks) {
        return FRAME_ERR_CHUNK_INVALID;
    }

    memcpy(&_buffer[chunk * _chunk_size], chunk_ptr, chunk_sz);
    return FRAME_OK;
}


int Frame::probeChunk(int chunk, char *(&chunk_ptr), int &chunk_sz)
{
    if (chunk >= _max_chunks) {
        return FRAME_ERR_CHUNK_INVALID;
    }

    chunk_sz = _chunk_size;
    chunk_ptr = &_buffer[chunk * _chunk_size];
    return FRAME_OK;
}


bool Frame::compareChunks(char *chunk1_ptr, int chunk1_sz, char *chunk2_ptr,
                          int chunk2_sz)
{
    assert(chunk1_ptr != NULL && chunk2_ptr != NULL);

    return (chunk1_sz == _chunk_size && chunk2_sz == _chunk_size &&
        memcmp(chunk1_ptr, chunk2_ptr, _chunk_size) == 0);
}


int Frame::setChunkData(int chunk, char *data_ptr, int data_sz)
{
    assert(data_ptr != NULL && data_sz > 0);

    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_CHUNK_UNALLOC;
    }

    if (chunk + 1 > num_chunks) {
        return FRAME_ERR_CHUNK_INVALID;
    }

    if (chunk == 0) {
        if (data_sz > _chunk_size - 1) {
            return FRAME_ERR_INSUFF_SPACE;
        }
        memcpy(&_buffer[FRAME_CHUNKS_FIELD + 1], data_ptr, data_sz);
    } else {
        if (data_sz > _chunk_size) {
            return FRAME_ERR_INSUFF_SPACE;
        }
        memcpy(&_buffer[chunk * _chunk_size], data_ptr, data_sz);
    }

    return FRAME_OK;
}


int Frame::getChunkData(int chunk, char *(&buff), int &buff_sz)
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (chunk >= num_chunks) {
        return FRAME_ERR_CHUNK_INVALID;
    }

    if (chunk == 0) {
        buff = &(_buffer[FRAME_CHUNKS_FIELD + 1]);
        buff_sz = _chunk_size - 1;
    } else {
        buff = &(_buffer[chunk * _chunk_size]);
        buff_sz = _chunk_size;
    }

    return FRAME_OK;
}


void Frame::dumpFrame(std::ostream &out)
{
    std::ios_base::fmtflags f(out.flags());
    for (int i = 0; i < _buffer_size; i++) {
        out << std::setfill('0') << std::setw(2) << std::hex
            << (0xff & (unsigned int)_buffer[i]);

        if ((i + 1) % _chunk_size == 0) {
            out << '\n';
        }
    }
    out << '\n';
    out.flags(f);
}


void Frame::printFrameInfo(std::ostream &out)
{
    out << "Num Chunks: " << (int) _buffer[FRAME_CHUNKS_FIELD] << std::endl;

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    switch (type)
    {
    case FRAME_TYPE_NULL:
        out << "Frame Type: NULL" << std::endl;
        break;

    case FRAME_TYPE_DATA:
        out << "Frame Type: DATA" << std::endl;
        unsigned int size_raw;
        memcpy((char*)&size_raw, &_buffer[DATA_FRAME_SIZE_FIELD], sizeof(size_raw));
        out << "Data Size: " << ntohl(size_raw) << std::endl;
        break;

    case FRAME_TYPE_CHAFF:
        out << "Frame Type: CHAFF" << std::endl;
        break;

    case FRAME_TYPE_CTRL:
        {
            out << "Frame Type: CTRL" << std::endl;
            unsigned int ctrl_type_raw;
            memcpy((char*)&ctrl_type_raw, &_buffer[CTRL_FRAME_TYPE_FIELD],
                sizeof(ctrl_type_raw));
            unsigned int ctrl_type = ntohl(ctrl_type_raw);
            unsigned int k_min_raw, k_min;


            switch (ctrl_type) {
                case FRAME_CTRL_TYPE_HELLO:
                    out << "Ctrl Type: HELLO" << std::endl;

                    memcpy((char*)&k_min_raw, &_buffer[CTRL_FRAME_HEADER_SIZE],
                        sizeof(k_min_raw));
                    k_min = ntohl(k_min_raw);

                    out << "\tK-min : " << k_min << std::endl;

                    break;

                case FRAME_CTRL_TYPE_HELLO_OK:
                    out << "Ctrl Type: HELLO OK" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_ACTIVE:
                    out << "Ctrl Type: ACTIVE" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_WAIT:
                    out << "Ctrl Type: WAIT" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_CHANGE:
                    out << "Ctrl Type: CHANGE" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_CHANGE_OK:
                    out << "Ctrl Type: CHANGE OK" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_INACTIVE:
                    out << "Ctrl Type: INACTIVE" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_SHUT:
                    out << "Ctrl Type: SHUT" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_SHUT_OK:
                    out << "Ctrl Type: SHUT OK" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_TS_RATE:
                    out << "Ctrl Type: TS RATE" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_ERR_HELLO:
                    out << "Ctrl Type: ERR HELLO" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_ERR_ACTIVE:
                    out << "Ctrl Type: ERR ACTIVE" << std::endl;
                    break;

                case FRAME_CTRL_TYPE_ERR_INACTIVE:
                    out << "Ctrl Type: ERR INACTIVE" << std::endl;
                    break;

                default:
                    out << "Ctrl Type: UNKNOWN " << std::to_string(ctrl_type)
                        << std::endl;
            }
        }
        break;

    default:
        out << "Frame Type: UNKNOWN (" << type << ")" << std::endl;
        break;
    }
}


void Frame::setFrameType(int type)
{
    assert(type == FRAME_TYPE_CHAFF ||
           type == FRAME_TYPE_DATA  ||
           type == FRAME_TYPE_CTRL);

    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        int status = allocChunk(1);
        assert(status == FRAME_OK);
    }
    _buffer[FRAME_TYPE_FIELD] = (unsigned char) type;
}


int Frame::getFrameType()
{
    return (int) _buffer[FRAME_TYPE_FIELD];
}


int Frame::setDataFrameData(char *data_ptr, int data_sz)
{
    assert(data_ptr != NULL && data_sz > 0);

    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_CHUNK_UNALLOC;
    }

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_DATA) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    int total_sz = data_sz + DATA_FRAME_HEADER_SIZE;

    if (total_sz > _buffer_size) {
        return FRAME_ERR_INSUFF_SPACE;
    }

    _buffer[FRAME_CHUNKS_FIELD] = total_sz / _chunk_size +
                                    ((total_sz % _chunk_size > 0)?1:0);

    unsigned int data_size = htonl(data_sz);
    memcpy(&_buffer[DATA_FRAME_SIZE_FIELD], &data_size, sizeof(data_size));
    memcpy(&_buffer[DATA_FRAME_HEADER_SIZE], data_ptr, data_sz);

    return FRAME_OK;
}


int Frame::getDataFrameData(char *(&data_ptr), int &data_sz)
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_DATA_UNAVAILABLE;
    }

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_DATA) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    unsigned int size_raw;
    memcpy((char*)&size_raw, &_buffer[DATA_FRAME_SIZE_FIELD], sizeof(size_raw));
    data_sz = ntohl(size_raw);
    data_ptr = &_buffer[DATA_FRAME_HEADER_SIZE];

    return FRAME_OK;
}


int Frame::getDataFrameSpace(char *(&data_ptr), int &space_sz)
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_DATA_UNAVAILABLE;
    }

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_DATA) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    space_sz = _buffer_size - DATA_FRAME_HEADER_SIZE;
    data_ptr = &_buffer[DATA_FRAME_HEADER_SIZE];
    return FRAME_OK;
}


int Frame::setDataFrameSize(int data_sz)
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_DATA_UNAVAILABLE;
    }

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_DATA) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    if (data_sz < 0 || data_sz > (_buffer_size - DATA_FRAME_HEADER_SIZE)) {
        return FRAME_ERR_WRONG_DATA_SIZE;
    }

    unsigned int data_size = htonl(data_sz);
    memcpy(&_buffer[DATA_FRAME_SIZE_FIELD], &data_size, sizeof(data_size));
    int total_sz = data_sz + DATA_FRAME_HEADER_SIZE;
    _buffer[FRAME_CHUNKS_FIELD] =
        total_sz / _chunk_size +
        ((total_sz % _chunk_size > 0)?1:0);

    return FRAME_OK;
}


int Frame::setChaffFrameData()
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_CHUNK_UNALLOC;
    }

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_CHAFF) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    for (int i = FRAME_TYPE_FIELD + 1; i < num_chunks * _chunk_size; i++) {
        _buffer[i] = (char) (0xff & rand());
    }

    return FRAME_OK;
}


int Frame::setCtrlFrameData(FrameControlFields *ctrl)
{
    assert(ctrl != NULL);

    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_CHUNK_UNALLOC;
    }

    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_CTRL) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    int data_sz;
    switch (ctrl->_type) {
        case FRAME_CTRL_TYPE_HELLO:
            data_sz = FRAME_CTRL_SIZE_HELLO;
            break;
        case FRAME_CTRL_TYPE_TS_RATE:
            data_sz = FRAME_CTRL_SIZE_TS_RATE;
            break;
        case FRAME_CTRL_TYPE_HELLO_OK:
        case FRAME_CTRL_TYPE_ACTIVE:
        case FRAME_CTRL_TYPE_WAIT:
        case FRAME_CTRL_TYPE_CHANGE:
        case FRAME_CTRL_TYPE_CHANGE_OK:
        case FRAME_CTRL_TYPE_INACTIVE:
        case FRAME_CTRL_TYPE_SHUT:
        case FRAME_CTRL_TYPE_SHUT_OK:
        case FRAME_CTRL_TYPE_ERR_ACTIVE:
        case FRAME_CTRL_TYPE_ERR_INACTIVE:
            data_sz = 0;
            break;
        default:
            return FRAME_ERR_WRONG_CTRL_TYPE;
    }

    int total_sz = data_sz + CTRL_FRAME_HEADER_SIZE;

    if (total_sz > _buffer_size) {
        return FRAME_ERR_INSUFF_SPACE;
    }

    _buffer[FRAME_CHUNKS_FIELD] = total_sz / _chunk_size +
                                    ((total_sz % _chunk_size > 0)?1:0);

    unsigned int ctrl_type = htonl(ctrl->_type);
    memcpy(&_buffer[CTRL_FRAME_TYPE_FIELD], &ctrl_type, sizeof(ctrl_type));

    /* Copying specific parameters */
    unsigned int k_min;
    unsigned int ts_rate;
    switch (ctrl->_type) {
        case FRAME_CTRL_TYPE_HELLO:
            k_min = htonl(ctrl->_k_min);
            memcpy(&_buffer[CTRL_FRAME_HEADER_SIZE], &k_min, sizeof(k_min));
        break;
        case FRAME_CTRL_TYPE_TS_RATE:
            ts_rate = htonl(ctrl->_ts_rate);
            memcpy(&_buffer[CTRL_FRAME_HEADER_SIZE], &ts_rate, sizeof(ts_rate));
    }

    return FRAME_OK;
}


int Frame::getCtrlFrameData(FrameControlFields &ctrl)
{
    int num_chunks = (int) _buffer[FRAME_CHUNKS_FIELD];
    if (num_chunks == 0) {
        return FRAME_ERR_DATA_UNAVAILABLE;
    }
    int type = (int) _buffer[FRAME_TYPE_FIELD];
    if (type != FRAME_TYPE_CTRL) {
        return FRAME_ERR_WRONG_FRAME_TYPE;
    }

    unsigned int ctrl_type_raw;
    memcpy((char*)&ctrl_type_raw, &_buffer[CTRL_FRAME_TYPE_FIELD], sizeof(ctrl_type_raw));
    unsigned int ctrl_type = ntohl(ctrl_type_raw);

    switch (ctrl_type) {
        case FRAME_CTRL_TYPE_HELLO:
            ctrl._type = FRAME_CTRL_TYPE_HELLO;

            unsigned int k_min_raw;
            memcpy((char*)&k_min_raw, &_buffer[CTRL_FRAME_HEADER_SIZE], sizeof(k_min_raw));
            ctrl._k_min = ntohl(k_min_raw);

            break;

        case FRAME_CTRL_TYPE_TS_RATE:
            ctrl._type = FRAME_CTRL_TYPE_TS_RATE;

            unsigned int ts_rate;
            memcpy((char*)&ts_rate, &_buffer[CTRL_FRAME_HEADER_SIZE], sizeof(ts_rate));
            ctrl._ts_rate = ntohl(ts_rate);

            break;

        case FRAME_CTRL_TYPE_HELLO_OK:
        case FRAME_CTRL_TYPE_ACTIVE:
        case FRAME_CTRL_TYPE_WAIT:
        case FRAME_CTRL_TYPE_CHANGE:
        case FRAME_CTRL_TYPE_CHANGE_OK:
        case FRAME_CTRL_TYPE_INACTIVE:
        case FRAME_CTRL_TYPE_SHUT:
        case FRAME_CTRL_TYPE_SHUT_OK:
        case FRAME_CTRL_TYPE_ERR_HELLO:
        case FRAME_CTRL_TYPE_ERR_ACTIVE:
        case FRAME_CTRL_TYPE_ERR_INACTIVE:
            ctrl._type = ctrl_type;
            break;
        default:
            return FRAME_ERR_WRONG_CTRL_TYPE;
    }

    return FRAME_OK;
}
