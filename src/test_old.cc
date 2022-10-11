#include <queue>
#include <thread>
#include <iostream>
#include <mutex>
#include <condition_variable>

std::queue<int> dataQueue;
std::mutex queueMutex;
std::condition_variable queueConditionVariable;

void producerFunction() {
  // This function will keep generating data forever
  int sleepSeconds;
  int newNumber;
  int otherNumber;
  while (true) {
    // Wait from 1 to 3 seconds before generating data
    sleepSeconds = rand() % 3 + 1;
    std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));

    // Add a number to the queue
    newNumber = rand() % 100 + 1; // Random number from 1 to 100
    otherNumber = rand() % 100 + 1; // Random number from 1 to 100
    std::lock_guard<std::mutex> g(queueMutex);
    dataQueue.push(newNumber);
    dataQueue.push(otherNumber);

    std::cout << "Added numbers to queue: " << newNumber << ", " << otherNumber
              << std::endl;

    // Notify one thread that the condition variable might have changed
    queueConditionVariable.notify_one();
  }
}

void consumerFunction() {
  // This function will consume data forever
  while (true) {
    int numberToProcess = 0;

    // We only need to lock the mutex for the time it takes us to pop an item
    // out. Adding this scope releases the lock right after we poped the item
    {
      // Condition variables need a unique_lock instead of a lock_guard, because
      // the mutex might be locked and unlocked multiple times. By default, this
      // line will lock the mutex
      std::unique_lock<std::mutex> g(queueMutex);

      // This call to `wait` will first check if the contion is met. i.e. If
      // the queue is not empty.
      // If the queue is not empty, the execution of the code will continue
      // If the queue is empty, it will unlock the mutex and wait until a signal
      // is sent to the condition variable. When the signal is sent, it will
      // acquire the lock and check the condition again.
      queueConditionVariable.wait(g, []{ return !dataQueue.empty(); });

      // We don't need to check if the queue is empty anymore, because the
      // Condition Variable does that for us
      numberToProcess = dataQueue.front();
      dataQueue.pop();
    }

    // Only process if there are numbers
    if (numberToProcess) {
      std::cout << "Processing number: " << numberToProcess << std::endl;
    }
  }
}

int main() {
  std::thread producer(producerFunction);
  std::thread consumer1(consumerFunction);

  producer.join();
  consumer1.join();
}