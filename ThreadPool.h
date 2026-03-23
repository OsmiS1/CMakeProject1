#pragma once

#define _HAS_STD_BYTE 0

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>

using namespace std;

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) : m_stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this] {
                while (true) {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(m_queueMutex);
                        m_condition.wait(lock, [this] {
                            return m_stop || !m_tasks.empty();
                            });

                        if (m_stop && m_tasks.empty()) {
                            return;
                        }

                        task = move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task();
                }
                });
        }
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> future<typename invoke_result<F, Args...>::type> {
        using return_type = typename invoke_result<F, Args...>::type;

        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        );

        future<return_type> result = task->get_future();

        {
            unique_lock<mutex> lock(m_queueMutex);
            if (m_stop) {
                throw runtime_error("enqueue on stopped ThreadPool");
            }
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_condition.notify_one();
        return result;
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> lock(m_queueMutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (auto& worker : m_workers) {
            worker.join();
        }
    }

private:
    vector<thread> m_workers;
    queue<function<void()>> m_tasks;
    mutex m_queueMutex;
    condition_variable m_condition;
    bool m_stop;
};