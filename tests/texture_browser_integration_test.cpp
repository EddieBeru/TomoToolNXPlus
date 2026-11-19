#include "support/texture_read_shim.h"
#include "texture_browser_session.hpp"
#include <condition_variable>
#include <chrono>
#include <zstd.h>

struct Queue {
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<TextureBrowser::Session::Completion> items;
    void push(TextureBrowser::Session::Completion c) {
        std::lock_guard<std::mutex> lock(mutex);items.push_back(std::move(c));ready.notify_all();
    }
    void drain() {
        std::unique_lock<std::mutex> lock(mutex);
        assert(ready.wait_for(lock,std::chrono::seconds(5),[&]{return !items.empty();}));
        auto next=std::move(items);items.clear();lock.unlock();
        for(auto& c:next)c();
    }
};
int main() {
    using namespace TextureBrowser;
    for(uint64_t account:{1u,2u}) {
        std::vector<uint8_t> raw(131072,uint8_t(account)), frame(ZSTD_compressBound(raw.size()));
        frame.resize(ZSTD_compress(frame.data(),frame.size(),raw.data(),raw.size(),1));
        saves[account]["/Ugc/UgcFood000.ugctex.zs"]=frame;
        auto names=Fixtures::names();names[0x44]=uint8_t('A'+account-1);
        saves[account]["/Player.sav"]=names;
    }
    const auto snapshot=saves;
    Queue queue;Model model;TextureBrowser::Result<Preview> preview;int deliveries=0;
    {
        Session session(createReadSource(),[&](Session::Completion c){queue.push(std::move(c));});
        auto catalog=[&](uint64_t key){
            model.select({{{key,0}},"Synthetic",size_t(key),false});
            session.submit([&,key](ReadSource& source,const Cancel& cancel){
                auto result=loadCatalog(source,{{key,0}},cancel);
                return [&,key,result=std::move(result)]() mutable {
                    assert(model.context.account.key.words[0]==key);
                    model.accept(std::move(result));++deliveries;
                };
            });
        };
        catalog(1);queue.drain();assert(model.state==State::Ready);
        assert(model.catalog.entries[0].name=="A");
        auto entry=model.catalog.entries[0];
        session.submit([&,entry](ReadSource& source,const Cancel& cancel){
            auto result=loadPreview(source,entry,cancel);
            return [&,result=std::move(result)]() mutable {preview=std::move(result);};
        });
        queue.drain();assert(preview && !preview.value.rgba.empty());
        const auto pixelsA=preview.value.rgba;
        shortRead=true;
        auto requestPreview=[&]{
            session.submit([&,entry](ReadSource& source,const Cancel& cancel){
                auto result=loadPreview(source,entry,cancel);
                return [&,result=std::move(result)]() mutable {preview=std::move(result);};
            });
        };
        requestPreview();queue.drain();assert(!preview && preview.value.rgba.empty());
        shortRead=false;requestPreview();queue.drain();assert(preview && preview.value.rgba==pixelsA);
        // Refresh fails completely, then retry succeeds after the source recovers.
        interrupted=true;catalog(1);queue.drain();assert(model.state==State::CatalogError);
        interrupted=false;catalog(1);queue.drain();assert(model.state==State::Ready);
        // Hold an actual libnx read, cancel it and switch accounts before release.
        std::mutex mutex;std::condition_variable gate;bool entered=false,released=false;
        beforeRead=[&]{
            std::unique_lock<std::mutex> lock(mutex);entered=true;gate.notify_all();
            assert(gate.wait_for(lock,std::chrono::seconds(5),[&]{return released;}));
        };
        catalog(1);
        {std::unique_lock<std::mutex> lock(mutex);assert(gate.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}));}
        session.cancel();catalog(2);
        {std::lock_guard<std::mutex> lock(mutex);released=true;gate.notify_all();}
        queue.drain();assert(model.state==State::Ready && model.catalog.entries[0].name=="B");
        beforeRead={};assert(deliveries==4);
        entry=model.catalog.entries[0];
        session.submit([&,entry](ReadSource& source,const Cancel& cancel){
            auto result=loadPreview(source,entry,cancel);
            return [&,result=std::move(result)]() mutable {preview=std::move(result);};
        });
        queue.drain();assert(preview && preview.value.rgba!=pixelsA);
        // An already queued completion cannot publish after leaving the browser.
        session.submit([](ReadSource&,const Cancel&){return []{assert(false);};});
        {std::unique_lock<std::mutex> lock(queue.mutex);assert(queue.ready.wait_for(lock,std::chrono::seconds(5),[&]{return !queue.items.empty();}));}
        session.stop();queue.drain();
    }
    assert(saves==snapshot && !filesystemOpen && filesOpen==0 && dirsOpen==0);
    for(const auto& call:calls)assert(call.find("write")==std::string::npos && call.find("commit")==std::string::npos);
    std::cout<<"browser integrated read-only lifecycle: OK\n";
}
