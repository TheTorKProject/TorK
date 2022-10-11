#include <thread>
#include <map>
#include "common/RingBuffer.hh"
#include "controller/Frame.hh"
#include "controller/FramePool.hh"
//#include "common/ThreadPool.hh"


void producerFunction(RingBuffer<int>* rb)
{
    int sleepSeconds;
    int newNumber;
    int otherNumber;
    int status;

    while (true) {

        sleepSeconds = rand() % 3 + 1;
        std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));

        newNumber = rand() % 100 + 1; // Random number from 1 to 100
        otherNumber = rand() % 100 + 1; // Random number from 1 to 100

        rb->put(newNumber, &status);
        if (status != RINGBUFFER_STATUS_OK) {
            std::cout << "[P] Failed to add " << newNumber << " to ring buffer." << std::endl;
            return;
        }

        rb->put(otherNumber, &status);
        if (status != RINGBUFFER_STATUS_OK) {
            std::cout << "[P] Failed to add " << otherNumber << " to ring buffer." << std::endl;
            return;
        }

        std::cout << "[P] Added numbers to queue: " << newNumber << ", " << otherNumber
                << std::endl;
    }
}

void consumerFunction(RingBuffer<int>* rb)
{
    int status;

    while (true) {

        int numberToProcess = rb->get(&status);
        if (status != RINGBUFFER_STATUS_OK) {
            std::cout << "[C] Failed to get number from ring buffer." << std::endl;
            return;
        }

        std::cout << "[C] Processing number: " << numberToProcess << std::endl;
    }
}

int main_ring_buffer()
{
    RingBuffer<int> rb (10);

    rb.enable();

    std::thread producer(producerFunction, &rb);
    std::thread consumer(consumerFunction, &rb);

    std::cout << "[M] Sleeping." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "[M] Disabling." << std::endl;
    rb.disable();

    producer.join();
    consumer.join();

    std::cout << "The end." << std::endl;
    return 0;
}

/* void testFramePool() {
    auto a = std::make_shared<std::packaged_task<int()> > (
        std::bind(testThread, 4)
    );

    (*(a))();

    std::future<int> result = a->get_future();
    std::cout << "B: " << result.get() << std::endl;
    A obj_a;
    ThreadPool thread_pool;
    thread_pool.initialize(4);
    std::future_status status;

    auto a = thread_pool.submit(std::bind(&A::tA, &obj_a, 4));

    auto b = thread_pool.submit(testThread, 2);
    auto c = thread_pool.submit(testThread, 3);

    std::map<int, int> m;

    m.insert(std::make_pair(1,2));

    int x = m[1];
    std::cout << "X: " << x << std::endl;
    m[1] = 3;
    x = m[1];
    std::cout << "X: " << x << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    std::future<int> b;
    if (b.valid()) {
        status = b.wait_for(std::chrono::seconds(0));

        if (status == std::future_status::deferred) {
            std::cout << "Deferred!" << std::endl;
        } else if (status == std::future_status::timeout) {
            std::cout << "Timeout!" << std::endl;
        } else if (status == std::future_status::ready) {
            std::cout << "Ready!" << std::endl;
        }
    }


    // get result from future
    std::cout << result.get() << std::endl;

    Frame *frame = nullptr;
    char data[] = "ABC";

    FramePool pool(4, 1);

    pool.printFramePoolInfo();

    for (int i = 0; i < 5; i++) {
        std::cout << "Alloc " << i + 1 << std::endl;
        assert(pool.allocFrame(frame) == FRAME_POOL_OK);
    }

    pool.printFramePoolInfo();

} */

int testThread(int arg) {
    std::cout << "Hei there " << arg << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "Finished!" << std::endl;
    return arg;
}

class A {
    public:
        int tA(int arg) {
            std::cout << "Hei there " << arg << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            std::cout << "Finished!" << std::endl;
            return arg;
        }
};

int main()
{

    Frame f(2, 100);

    std::cout << "Num chunks:" << f.getNumChunks() << std::endl;

    f.allocChunk(2);
    int c1 = 0;
    int c2 = 1;

    std::cout << "Num chunks:" << f.getNumChunks() << std::endl;

    std::cout << "Chunks allocated:" << c1 << " " << c2 << std::endl;

    char *b1; int b1_sz;
    f.getChunkData(c1, b1, b1_sz);

    for (int i = 0; i < b1_sz; i++) {
        b1[i] = 0x11;
    }

    std::cout << "Chunk c1 size:" << b1_sz << std::endl;
    f.dumpFrame();

    char *b2; int b2_sz;
    f.getChunkData(c2, b2, b2_sz);

    for (int i = 0; i < b2_sz; i++) {
        b2[i] = 0xee;
    }

    std::cout << "Chunk c2 size:" << b2_sz << std::endl;
    f.dumpFrame();

    char d[5];
    for (int i = 0; i < sizeof(d); i++) {
        d[i] = 0xff;
    }

    f.setFrameType(FRAME_TYPE_DATA);
    f.setDataFrameData(d, sizeof(d));
    std::cout << "Data updated." << std::endl;
    f.dumpFrame();

    char *d_ptr;
    int d_sz;
    f.getDataFrameData(d_ptr, d_sz);
    std::cout << "Data read:" << d_sz << std::endl;


    Frame f2(2, 100);

    char d2[30];
    for (int i = 0; i < sizeof(d2); i++) {
        d2[i] = 0xaa;
    }

    f2.setFrameType(FRAME_TYPE_DATA);
    f2.setDataFrameData(d2, sizeof(d2));
    std::cout << "f2 Data updated." << std::endl;
    f2.dumpFrame();

    char *d_ptr2; int d_read;
    f2.getDataFrameData(d_ptr2, d_read);
    if (memcmp(d_ptr2, d2, d_read) != 0 || d_read != sizeof(d2)) {
        std::cout << "f2 Data corrupted." << std::endl;
    } else {
        std::cout << "f2 Data incorrupted." << std::endl;
    }

    Frame f3(2, 100);
    f3.setFrameType(FRAME_TYPE_CHAFF);
    f3.setChaffFrameData();
    std::cout << "f3 Chaff frame." << std::endl;
    f3.dumpFrame();

    Frame f4(1, 100);
    f4.setFrameType(FRAME_TYPE_CTRL);

    FrameControlFields ctrl;
    ctrl._type = FRAME_CTRL_TYPE_HELLO;
    f4.setCtrlFrameData(&ctrl);
    std::cout << "f4 Hello frame." << std::endl;
    f4.dumpFrame();

    FrameControlFields ctrl_read;
    f4.getCtrlFrameData(ctrl_read);
    std::cout << "f4 Frame type:" << ctrl_read._type << std::endl;

    char *chunk1; int chunk1_sz;
    f4.probeChunk(0, chunk1, chunk1_sz);

    Frame f5(2, 100);
    f5.assignChunk(0, chunk1, chunk1_sz);
    std::cout << "f5 frame." << std::endl;
    f5.dumpFrame();

    char *chunk2; int chunk2_sz;
    f5.probeChunk(0, chunk2, chunk2_sz);
    if (f5.compareChunks(chunk1, chunk1_sz, chunk2, chunk2_sz)) {
        std::cout << "f4 and f5 chunks equal." << std::endl;
    } else {
        std::cout << "f4 and f5 chunks not equal." << std::endl;
    }

    Frame f6(1, 100);
    char *din_ptr; int din_sz; int space_sz;
    int stat = 0;
    f6.setFrameType(FRAME_TYPE_DATA);
    stat += f6.getDataFrameSpace(din_ptr, space_sz);
    std::cout << "f6 stat:" << stat <<  "; size:" << space_sz << std::endl;
    for (int i = 0; i < space_sz - 4; i++) {
        din_ptr[i] = 0xcc;
    }

    std::cout << "f6 frame." << std::endl;
    f6.dumpFrame();

    stat += f6.setDataFrameSize(space_sz - 4);
    std::cout << "f6 stat:" << stat << std::endl;
    assert(stat == FRAME_OK);

    std::cout << "f6 frame." << std::endl;
    f6.dumpFrame();
    f6.printFrameInfo();

    std::cout << "The end." << std::endl;
}

