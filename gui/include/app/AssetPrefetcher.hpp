#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Owns the background threads that warm the asset cache. Each job runs on
// its own thread; the destructor joins them all, so no thread can outlive
// the cache it writes into (the cache is a function-local static).
class AssetPrefetcher {
public:
    AssetPrefetcher() = default;
    AssetPrefetcher(const AssetPrefetcher &) = delete;
    AssetPrefetcher &operator=(const AssetPrefetcher &) = delete;
    ~AssetPrefetcher();

    // Starts fetching `paths` with `concurrency` workers. Returns a job id
    // usable with done() / total() / wait().
    std::size_t start(std::vector<std::string> paths, int concurrency);

    std::size_t done(std::size_t job) const;
    std::size_t total(std::size_t job) const;
    std::atomic<std::size_t> &progress(std::size_t job);
    void wait(std::size_t job);

private:
    struct Job {
        std::vector<std::string> paths;
        std::atomic<std::size_t> done{0};
        std::thread              worker;
    };
    std::vector<std::unique_ptr<Job>> _jobs;
};
