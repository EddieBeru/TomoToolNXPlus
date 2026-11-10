#include "texture_browser_session.hpp"
namespace TextureBrowser {
Session::Session(std::unique_ptr<ReadSource> source,Deliver deliver):
    source(std::move(source)),deliver(std::move(deliver)) {worker=std::thread([this]{run();});}
Session::~Session() {stop();if(worker.joinable())worker.join();}
void Session::submit(Job job) {
    std::lock_guard<std::mutex> l(mutex);
    if(stopping)return;
    pendingGeneration=++gate->generation;
    pending=std::move(job);wake.notify_one();
}
void Session::cancel() {
    std::lock_guard<std::mutex> l(mutex);
    ++gate->generation; pending={};
}
void Session::stop() {
    std::lock_guard<std::mutex> l(mutex);
    gate->alive=false; ++gate->generation; pending={};stopping=true;wake.notify_one();
}
bool Session::finished() const {return done.load();}
void Session::run() {
    for(;;) {
        Job job; uint64_t generation;
        {
            std::unique_lock<std::mutex> l(mutex);
            wake.wait(l,[this]{return stopping || bool(pending);});
            if(stopping)break;
            job=std::move(pending); pending={};generation=pendingGeneration;
        }
        auto current=gate;
        Cancel cancel=[current,generation]{return !current->alive || current->generation!=generation;};
        auto completion=job(*source,cancel);
        source->close();
        if(completion && !cancel()) {
            std::weak_ptr<Gate> weak=current;
            deliver([weak,generation,completion=std::move(completion)] {
                auto locked=weak.lock();
                if(locked && locked->alive && locked->generation==generation)completion();
            });
        }
    }
    source->close(); done=true;
}
}
