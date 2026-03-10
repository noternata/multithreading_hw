#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <cstddef> 


template <typename T>
void ApplyFunction(std::vector<T>& data,
                   const std::function<void(T&)>& transform,
                   const int threadCount = 1){
    if (data.empty()) {
        return;
    }
    // кажется можно поменять тип на size_t
    int actualThreads = threadCount;
    // 0 потоков 
    if (actualThreads < 1) {
        actualThreads = 1;
    }
    // потоков больше чем элементов 
    if (actualThreads > static_cast<int>(data.size())) {
        actualThreads = static_cast<int>(data.size());
    }
    // для обработки потоком части вектора 
    auto applyTransform = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            transform(data[i]);
        }
    };
    // если один поток 
    if (actualThreads == 1) {
        applyTransform(0, data.size());
        return;
    }
    // создаем вектор потоков и определяем сколько элементов обрабатывается в потоке
    std::vector<std::thread> threads;
    // резервируем память 
    threads.reserve(actualThreads);
    // сколько элементов в потоке
    std::size_t baseSize = data.size() / actualThreads;
    std::size_t left = data.size() % actualThreads;
    std::size_t begin = 0;
    for (int t = 0; t < actualThreads; ++t) {
        std::size_t finalSize = baseSize;
        // распределяем остаток по первым потокам
        if (t < static_cast<int>(left)) {
            finalSize += 1;
        }
        std::size_t end = begin + finalSize;
        // создаем новый поток
        threads.emplace_back(applyTransform, begin, end);
        //threads.push_back(std::thread(applyTransform, begin, end));
        // сдвигаем начало для следующего потока
        begin = end;
    }
    // ожидание заверщения всех потоков
    for (auto& th : threads) {
        th.join();
    }
}