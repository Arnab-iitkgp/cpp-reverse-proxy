#pragma once
#include<vector>
#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<functional> // allow us to pass fn as var

class ThreadPool{
private:
    std::vector<std::thread>workers;
    std::queue<std::function<void()>>tasks;
    std::mutex queue_mutex; // lock
    std::condition_variable condition; // like alarm clock, wake up or notify threads  

    bool stop;
public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    void enqueue(std::function<void()>task);
};