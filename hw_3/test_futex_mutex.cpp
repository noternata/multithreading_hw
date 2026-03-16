#include <gtest/gtest.h>
#include "futex_mutex.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

TEST(FutexMutexTest, TryLockUnlockTryLock) {
    FutexMutex mutex;

    EXPECT_TRUE(mutex.try_lock());
    EXPECT_FALSE(mutex.try_lock());

    mutex.unlock();

    EXPECT_TRUE(mutex.try_lock());
    mutex.unlock();
}

// один поток блокирует другой
TEST(FutexMutexTest, LockBlocksOtherThreadUntilUnlock) {
    FutexMutex mutex;
    std::atomic<bool> second_thread_acquired{false};

    mutex.lock();

    std::thread t([&]() {
        mutex.lock();
        second_thread_acquired.store(true, std::memory_order_relaxed);
        mutex.unlock();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(second_thread_acquired.load(std::memory_order_relaxed));

    mutex.unlock();

    t.join();

    EXPECT_TRUE(second_thread_acquired.load(std::memory_order_relaxed));
}

// конкуренция 
TEST(FutexMutexTest, ProtectsSharedCounter) {
    FutexMutex mutex;
    int counter = 0;

    constexpr int kThreadCount = 4;
    constexpr int kIterationsPerThread = 10000;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < kIterationsPerThread; ++j) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter, kThreadCount * kIterationsPerThread);
}