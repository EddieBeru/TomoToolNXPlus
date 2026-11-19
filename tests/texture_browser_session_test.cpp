#include "texture_browser_session.hpp"
#include "support/texture_browser_fixtures.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
int main() {
    using namespace TextureBrowser;
    std::mutex m; std::condition_variable ready; std::vector<Session::Completion> queue; std::atomic<int> worked{0};
    Session s(std::make_unique<Fixtures::Source>(),[&](Session::Completion c) {
        std::lock_guard<std::mutex> l(m);queue.push_back(std::move(c));ready.notify_all();
    });
    int delivered=0;
    s.submit([&](ReadSource&,const Cancel&){++worked;return [&]{++delivered;};});
    {std::unique_lock<std::mutex> l(m);assert(ready.wait_for(l,std::chrono::seconds(5),[&]{return !queue.empty();}));}
    s.cancel();
    {std::lock_guard<std::mutex> l(m);assert(queue.size()==1);queue[0]();queue.clear();}
    assert(delivered==0 && worked==1);
    s.submit([&](ReadSource&,const Cancel&){++worked;return [&]{++delivered;};});
    {std::unique_lock<std::mutex> l(m);assert(ready.wait_for(l,std::chrono::seconds(5),[&]{return !queue.empty();}));}
    {std::lock_guard<std::mutex> l(m);assert(queue.size()==1);queue[0]();queue.clear();}
    assert(delivered==1);
    std::mutex gateMutex;std::condition_variable gate;bool started=false,release=false;
    s.submit([&](ReadSource&,const Cancel&){
        std::unique_lock<std::mutex> lock(gateMutex);started=true;gate.notify_all();
        assert(gate.wait_for(lock,std::chrono::seconds(5),[&]{return release;}));
        return [&]{delivered+=100;};
    });
    {std::unique_lock<std::mutex> lock(gateMutex);assert(gate.wait_for(lock,std::chrono::seconds(5),[&]{return started;}));}
    s.submit([&](ReadSource&,const Cancel&){return [&]{delivered+=1000;};});
    s.submit([&](ReadSource&,const Cancel&){return [&]{++delivered;};});
    {std::lock_guard<std::mutex> lock(gateMutex);release=true;gate.notify_all();}
    {std::unique_lock<std::mutex> l(m);assert(ready.wait_for(l,std::chrono::seconds(5),[&]{return !queue.empty();}));}
    {std::lock_guard<std::mutex> l(m);assert(queue.size()==1);queue[0]();queue.clear();}
    assert(delivered==2);
    s.stop();
    std::cout<<"browser session: OK\n";
}
