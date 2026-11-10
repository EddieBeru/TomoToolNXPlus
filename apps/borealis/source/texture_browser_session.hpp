#pragma once
#include "texture_browser.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace TextureBrowser {
class Session {
public:
    using Completion=std::function<void()>;
    using Job=std::function<Completion(ReadSource&,const Cancel&)>;
    using Deliver=std::function<void(Completion)>;
    Session(std::unique_ptr<ReadSource> source,Deliver deliver);
    ~Session();
    void submit(Job job);
    void cancel();
    void stop();
    bool finished() const;
private:
    struct Gate {std::atomic<uint64_t> generation{0};std::atomic<bool> alive{true};};
    std::shared_ptr<Gate> gate=std::make_shared<Gate>();
    std::unique_ptr<ReadSource> source;
    Deliver deliver;
    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable wake;
    Job pending;
    uint64_t pendingGeneration=0;
    bool stopping=false;
    std::atomic<bool> done{false};
    void run();
};
}
