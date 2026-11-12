#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

struct NVGcontext {};

// Host model of action registration and ownership; never linked into Switch builds.
namespace brls {
struct Style {};
struct FrameContext {};
enum ControllerButton {BUTTON_B, BUTTON_START, BUTTON_LT, BUTTON_RT};
enum Sound {SOUND_NONE, SOUND_BACK};
enum class Visibility {VISIBLE, GONE};
enum class TransitionAnimation {NONE};
struct View;
struct Action {
    std::string label;
    std::function<bool(View*)> callback;
    bool hidden, repeating;
    Sound sound;
};
struct View {
    virtual ~View() = default;
    View* parent = nullptr;
    View* defaultFocus = this;
    Visibility visibility=Visibility::VISIBLE;
    std::map<ControllerButton,Action> actions;
    View* getParent() { return parent; }
    View* getDefaultFocus() { return defaultFocus; }
    void setVisibility(Visibility value) { visibility=value; }
    void registerAction(const std::string& label, ControllerButton button,
        const std::function<bool(View*)>& callback, bool hidden=false,
        bool repeating=false, Sound sound=SOUND_NONE) {
        actions.insert_or_assign(button,Action{label,callback,hidden,repeating,sound});
    }
    virtual void willDisappear(bool) {}
    virtual void willAppear(bool) {}
};
struct Box:View {
    virtual void draw(NVGcontext*,float,float,float,float,Style,FrameContext*) {}
};
struct BottomBar:Box {
    std::map<std::string,View> widgets;
    BottomBar() {
        for(auto* id:{"brls/battery","brls/wireless","brls/hints/time"}) widgets.try_emplace(id);
    }
    View* getView(const std::string& id) {return &widgets.at(id);}
};
struct Activity {
    explicit Activity(View* content):content(content) {}
    virtual ~Activity()=default;
    View* content;
    View* getContentView() { return content; }
    virtual void onPause() {}
    virtual void onResume() {}
};
struct Application {
    inline static std::vector<Activity*> activities;
    inline static View* currentFocus = nullptr;
    inline static std::map<std::string,std::function<View*()>> factories;
    static std::vector<Activity*> getActivitiesStack() { return activities; }
    static View* getCurrentFocus(){return currentFocus;}
    static void giveFocus(View* view) { currentFocus = view->getDefaultFocus(); }
    static void registerXMLView(const std::string& id,std::function<View*()> factory) {
        factories.insert_or_assign(id,std::move(factory));
    }
    static void pushActivity(Activity* activity,TransitionAnimation) {
        activities.push_back(activity);
        giveFocus(activity->getContentView());
    }
    static void popActivity(TransitionAnimation) {activities.pop_back();}
};
inline std::string getStr(const std::string& key){return key;}
}
