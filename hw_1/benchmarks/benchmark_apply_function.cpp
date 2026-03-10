#include <benchmark/benchmark.h>
#include "apply_func.h"


static void BM_Single_Light_SmallVector(benchmark::State& state) {
    // получаем длинну вектора 
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    std::function<void(int&)> transform = [](int& x) {
        x += 1;
    };

    for (auto _ : state) {
        // создаем вектор длинны n и применяем ApplyFunction 1 поток
        std::vector<int> data(n, 1);
        ApplyFunction(data, transform, 1);
        benchmark::DoNotOptimize(data);
    }
}
// используем UseRealTime чтобы оценить именно реалное время а не 
// CPUTime сколько суммарно процессорного времени съели все потоки
BENCHMARK(BM_Single_Light_SmallVector)->Arg(100)->UseRealTime();

static void BM_Multi_Light_SmallVector(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    std::function<void(int&)> transform = [](int& x) {
        x += 1;
    };

    for (auto _ : state) {
        std::vector<int> data(n, 1);
        ApplyFunction(data, transform, 4);
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_Multi_Light_SmallVector)->Arg(100)->UseRealTime();

static void BM_Single_Heavy_BigVector(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    std::function<void(int&)> transform = [](int& x) {
        for (int i = 0; i < 1000; ++i) {
            x = (x * 1664525 + 1013904223) % 1000000007;
        }
    };

    for (auto _ : state) {
        std::vector<int> data(n, 1);
        ApplyFunction(data, transform, 1);
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_Single_Heavy_BigVector)->Arg(100000)->UseRealTime();

static void BM_Multi_Heavy_BigVector(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    std::function<void(int&)> transform = [](int& x) {
        for (int i = 0; i < 1000; ++i) {
            x = (x * 1664525 + 1013904223) % 1000000007;
        }
    };

    for (auto _ : state) {
        std::vector<int> data(n, 1);
        ApplyFunction(data, transform, 4);
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_Multi_Heavy_BigVector)->Arg(100000)->UseRealTime();

BENCHMARK_MAIN();