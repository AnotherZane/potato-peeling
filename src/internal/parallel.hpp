#pragma once

// Running batches of independent tasks on a fixed set of threads.

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <CGAL/config.h>

// Tasks share lazy-exact numbers, whose exact values are computed on first use;
// CGAL makes that thread-safe only with CGAL_HAS_THREADS.
#ifndef CGAL_HAS_THREADS
#error "potato_peeling needs CGAL_HAS_THREADS (CGAL's thread-safe lazy-exact numbers)"
#endif

namespace potato_peeling::internal {

    // `threads` from the options: 0 means one per hardware thread.
    inline unsigned resolve_threads(const unsigned threads) {
        return threads ? threads : std::max(1u, std::thread::hardware_concurrency());
    }

    // Threads started once and reused for every batch: starting threads per batch
    // costs more than a batch of short tasks.
    class ThreadPool {
    public:
        // `threads` includes the calling thread, which works on every batch. If the
        // system has fewer threads to give, the pool is smaller (see size()).
        explicit ThreadPool(const unsigned threads) {
            for (unsigned w = 1; w < threads; ++w) {
                try {
                    workers_.emplace_back([this, w] { loop(w); });
                } catch (const std::system_error &) {
                    break;
                }
            }
        }

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        ~ThreadPool() {
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                quit_ = true;
            }
            wake_.notify_all();
            for (auto &t : workers_) t.join();
        }

        // Threads, including the caller.
        unsigned size() const { return static_cast<unsigned>(workers_.size()) + 1; }

        // Runs task(i, w) for every i in [0, count) and returns when all are done;
        // w < size() identifies the thread (0 is the caller), for per-thread data.
        // Tasks must be independent and write only their own results. The first
        // exception a task throws is rethrown here, after the others have stopped
        // taking tasks. Not reentrant: tasks must not call run().
        template <class Task> void run(const std::size_t count, const Task &task) {
            if (workers_.empty() || count <= 1) {
                for (std::size_t i = 0; i < count; ++i) task(i, std::size_t{0});
                return;
            }
            const std::function<void(std::size_t, std::size_t)> f = std::cref(task);
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                task_ = &f;
                count_ = count;
                next_ = 0;
                failed_ = false;
                busy_ = workers_.size();
                ++batch_;
            }
            wake_.notify_all();
            work(0);
            std::unique_lock<std::mutex> lock(mutex_);
            done_.wait(lock, [&] { return busy_ == 0; });
            task_ = nullptr;
            if (error_) std::rethrow_exception(std::exchange(error_, nullptr));
        }

    private:
        void loop(const std::size_t w) {
            std::uint64_t seen = 0;
            for (;;) {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    wake_.wait(lock, [&] { return quit_ || batch_ != seen; });
                    if (quit_) return;
                    seen = batch_;
                }
                work(w);
                const std::lock_guard<std::mutex> lock(mutex_);
                if (--busy_ == 0) done_.notify_one();
            }
        }

        void work(const std::size_t w) {
            for (;;) {
                const std::size_t i = next_.fetch_add(1);
                if (i >= count_ || failed_) return;
                try {
                    (*task_)(i, w);
                } catch (...) {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    if (!error_) error_ = std::current_exception();
                    failed_ = true;
                    return;
                }
            }
        }

        std::vector<std::thread> workers_;
        std::mutex mutex_;
        std::condition_variable wake_, done_;
        bool quit_ = false;
        std::uint64_t batch_ = 0;  // which batch the workers last saw
        std::size_t busy_ = 0;     // workers still on the current batch
        // The current batch; written under the mutex before batch_ changes, so
        // the workers that see the new batch_ see these too.
        const std::function<void(std::size_t, std::size_t)> *task_ = nullptr;
        std::size_t count_ = 0;
        std::atomic<std::size_t> next_{0};
        std::atomic<bool> failed_{false};
        std::exception_ptr error_;
    };

}  // namespace potato_peeling::internal
