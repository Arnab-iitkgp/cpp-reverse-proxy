#include "thread_pool.h"

ThreadPool::ThreadPool(size_t threads):stop(false){

    // pre spawn all the worker threads

    for(size_t i=0;i<threads;i++){

        // each worker gets an infinite loop lambda fn
        workers.emplace_back([this]{ // 'this' is pointer to this particular threadpool object
            while(true){
                std::function<void()> task;
                {
                    // 1. lock the queue (mutual excl)
                    std::unique_lock<std::mutex> lock(this->queue_mutex);

                    //2. go to sleep if queuue is empty (0% cpu util)

                    // wake up only if a taks arrives 

                    this->condition.wait(lock, [this]{
                        return this->stop || !this->tasks.empty();
                    });

                    //3. if we are stopping and queue is empty, exit the thread

                    if(this->stop && this->tasks.empty())return;

                    //4. grab the task and remv from queue

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                    //5. the lock is auto released, when the block ends;
                    //6. execute the tasl )outside the lock so other threads can use thes queue?

                }
                task();
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.push(task);
    } // lock releases
    
    // Ring the alarm clock! Wake up ONE sleeping thread to process the new task
    condition.notify_one();
}
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    // Wake up ALL threads so they can see stop=true and gracefully exit
    condition.notify_all();
    
    for (std::thread &worker : workers) {
        worker.join(); // Wait for everyone to finish
    }
}