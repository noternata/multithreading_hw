#pragma once

#include "future.h"

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads_count) {
        if (threads_count == 0) {
            throw std::runtime_error("ThreadPool size must be greater than 0");
        }

        for (std::size_t i = 0; i < threads_count; ++i) {
            workers_.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this]() {
                            return stop_ || !tasks_.empty();
                        });

                        if (stop_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }

        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void Enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }

            tasks_.push(std::move(task));
        }

        cv_.notify_one();
    }

    template <class F, class... Args>
    auto Submit(F&& func, Args&&... args)
        -> Future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
        using ResultType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        auto state = std::make_shared<SharedState<ResultType>>();
        Future<ResultType> future(state);

        auto bound_task = [func = std::decay_t<F>(std::forward<F>(func)),
                           args_tuple = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...)]() mutable -> ResultType {
            return std::apply(
                [&func](auto&... unpacked_args) -> ResultType {
                    return std::invoke(func, unpacked_args...);
                },
                args_tuple
            );
        };

        auto task = [state, bound_task = std::move(bound_task)]() mutable {
            try {
                if constexpr (std::is_void_v<ResultType>) {
                    bound_task();
                    SetValue(state);
                } else {
                    ResultType result = bound_task();
                    SetValue(state, std::move(result));
                }
            } catch (...) {
                SetException(state, std::current_exception());
            }
        };

        Enqueue(std::move(task));
        return future;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};