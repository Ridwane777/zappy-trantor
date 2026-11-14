#include "app/AssetPrefetcher.hpp"
#include "AssetCache.hpp"

AssetPrefetcher::~AssetPrefetcher()
{
    for (auto &j : _jobs)
        if (j->worker.joinable())
            j->worker.join();
}

std::size_t AssetPrefetcher::start(std::vector<std::string> paths, int concurrency)
{
    auto job = std::make_unique<Job>();
    job->paths = std::move(paths);
    Job *raw = job.get();
    raw->worker = std::thread([raw, concurrency] {
        zappy::prefetch(raw->paths, &raw->done, concurrency);
    });
    _jobs.push_back(std::move(job));
    return _jobs.size() - 1;
}

std::size_t AssetPrefetcher::done(std::size_t job) const
{
    return _jobs.at(job)->done.load();
}

std::size_t AssetPrefetcher::total(std::size_t job) const
{
    return _jobs.at(job)->paths.size();
}

std::atomic<std::size_t> &AssetPrefetcher::progress(std::size_t job)
{
    return _jobs.at(job)->done;
}

void AssetPrefetcher::wait(std::size_t job)
{
    Job &j = *_jobs.at(job);
    if (j.worker.joinable())
        j.worker.join();
}
