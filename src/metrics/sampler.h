#pragma once
#include <atomic>
#include <thread>
#include "state.h"

class Sampler
{
public:
    explicit Sampler(AppState &s) : st(s) {}
    void start();
    void stop();

private:
    void run();

    AppState &st;
    std::atomic<bool> on{false};
    std::thread th;
};
