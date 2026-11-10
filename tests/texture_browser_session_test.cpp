#include "texture_browser_session.hpp"
#include "support/texture_browser_fixtures.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
int main() {
    using namespace TextureBrowser;
    std::mutex m; std::vector<Session::Completion> queue; std::atomic<int> worked{0};
    Session s(std::make_unique<Fixtures::Source>(),[&](Session::Completion c) {
        std::lock_guard<std::mutex> l(m);queue.push_back(std::move(c));
    });
    int delivered=0;
    s.submit([&](ReadSource&,const Cancel&){++worked;return [&]{++delivered;};});
    for(int i=0;i<1000;i++) { {std::lock_guard<std::mutex> l(m);if(!queue.empty())break;} std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    s.cancel();
    {std::lock_guard<std::mutex> l(m);assert(queue.size()==1);queue[0]();queue.clear();}
    assert(delivered==0 && worked==1);
    s.submit([&](ReadSource&,const Cancel& c) {
        while(!c()) std::this_thread::yield();
        return Session::Completion{};
    });
    s.cancel();
    s.submit([&](ReadSource&,const Cancel&){++worked;return [&]{++delivered;};});
    for(int i=0;i<1000;i++) { {std::lock_guard<std::mutex> l(m);if(!queue.empty())break;} std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    {std::lock_guard<std::mutex> l(m);assert(queue.size()==1);queue[0]();queue.clear();}
    assert(delivered==1);
    std::atomic<bool> started{false}, release{false};
    s.submit([&](ReadSource&,const Cancel&){
        started=true;while(!release)std::this_thread::yield();
        return [&]{delivered+=100;};
    });
    for(int i=0;i<1000 && !started;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(started);
    s.submit([&](ReadSource&,const Cancel&){return [&]{delivered+=1000;};});
    s.submit([&](ReadSource&,const Cancel&){return [&]{++delivered;};});
    release=true;
    for(int i=0;i<1000;i++){ {std::lock_guard<std::mutex> l(m);if(!queue.empty())break;} std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    {std::lock_guard<std::mutex> l(m);assert(queue.size()==1);queue[0]();queue.clear();}
    assert(delivered==2);
    s.stop();
    for(int i=0;i<1000 && !s.finished();++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(s.finished());
    std::cout<<"browser session: OK\n";
}
