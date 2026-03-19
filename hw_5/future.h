#pragma once

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T>
struct SharedState {
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<T> value;
    std::exception_ptr exception;
    bool ready = false;
    bool retrieved = false;
};

template <>
struct SharedState<void> {
    std::mutex mutex;
    std::condition_variable cv;
    std::exception_ptr exception;
    bool ready = false;
    bool retrieved = false;
};

template <class T>
class Future {
public:
    Future() = default;

    explicit Future(std::shared_ptr<SharedState<T>> state)
        : state_(std::move(state)) {
    }

    void Wait() const {
        if (!state_) {
            throw std::runtime_error("Future has no state");
        }

        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [this]() {
            return state_->ready;
        });
    }

    T Get() {
        Wait();

        std::lock_guard<std::mutex> lock(state_->mutex);

        if (state_->retrieved) {
            throw std::runtime_error("Future result already retrieved");
        }

        state_->retrieved = true;

        if (state_->exception) {
            std::rethrow_exception(state_->exception);
        }

        if (!state_->value.has_value()) {
            throw std::runtime_error("SharedState has no value");
        }

        T result = std::move(*(state_->value));
        state_->value.reset();
        return result;
    }

private:
    std::shared_ptr<SharedState<T>> state_;
};

template <>
class Future<void> {
public:
    Future() = default;

    explicit Future(std::shared_ptr<SharedState<void>> state)
        : state_(std::move(state)) {
    }

    void Wait() const {
        if (!state_) {
            throw std::runtime_error("Future has no state");
        }

        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [this]() {
            return state_->ready;
        });
    }

    void Get() {
        Wait();

        std::lock_guard<std::mutex> lock(state_->mutex);

        if (state_->retrieved) {
            throw std::runtime_error("Future result already retrieved");
        }

        state_->retrieved = true;

        if (state_->exception) {
            std::rethrow_exception(state_->exception);
        }
    }

private:
    std::shared_ptr<SharedState<void>> state_;
};

template <class T>
void SetValue(const std::shared_ptr<SharedState<T>>& state, T value) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        if (state->ready) {
            throw std::runtime_error("SharedState already satisfied");
        }

        state->value = std::move(value);
        state->ready = true;
    }

    state->cv.notify_all();
}

inline void SetValue(const std::shared_ptr<SharedState<void>>& state) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        if (state->ready) {
            throw std::runtime_error("SharedState already satisfied");
        }

        state->ready = true;
    }

    state->cv.notify_all();
}

template <class T>
void SetException(const std::shared_ptr<SharedState<T>>& state, std::exception_ptr exception) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        if (state->ready) {
            throw std::runtime_error("SharedState already satisfied");
        }

        state->exception = exception;
        state->ready = true;
    }

    state->cv.notify_all();
}

inline void SetException(const std::shared_ptr<SharedState<void>>& state, std::exception_ptr exception) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        if (state->ready) {
            throw std::runtime_error("SharedState already satisfied");
        }

        state->exception = exception;
        state->ready = true;
    }

    state->cv.notify_all();
}