#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct NVGcolor {
    unsigned char r = 0, g = 0, b = 0, a = 255;
};

struct NVGpaint {};

struct NVGcontext {
    int nextTexture = 1;
    bool failCreate = false;
    int createCalls = 0;
    int drawCalls = 0;
    std::map<int, std::pair<int, int>> textureSizes;
    int nextFont = 100;
    std::map<int, std::vector<int>> fontFallbacks;
};

inline int nvgAddFallbackFontId(NVGcontext* ctx, int baseFont, int fallbackFont) {
    if (!ctx || baseFont < 0 || fallbackFont < 0)
        return 0;
    ctx->fontFallbacks[baseFont].push_back(fallbackFont);
    return 1;
}

inline int nvgCreateImageRGBA(NVGcontext* ctx, int width, int height, int, const unsigned char*) {
    if (!ctx || ctx->failCreate || width <= 0 || height <= 0)
        return 0;
    ++ctx->createCalls;
    const int id = ctx->nextTexture++;
    ctx->textureSizes[id] = {width, height};
    return id;
}
inline void nvgSave(NVGcontext*) {}
inline void nvgRestore(NVGcontext*) {}
inline void nvgIntersectScissor(NVGcontext*, float, float, float, float) {}
inline void nvgBeginPath(NVGcontext*) {}
inline void nvgRect(NVGcontext*, float, float, float, float) {}
inline NVGcolor nvgRGB(unsigned char r, unsigned char g, unsigned char b) { return {r, g, b, 255}; }
inline void nvgFillColor(NVGcontext*, NVGcolor) {}
inline void nvgFill(NVGcontext* ctx) {
    if (ctx)
        ++ctx->drawCalls;
}
inline NVGpaint nvgImagePattern(NVGcontext*, float, float, float, float, float, int, float) { return {}; }
inline void nvgFillPaint(NVGcontext*, NVGpaint) {}

// Host model of action registration, view hierarchy, focus and ownership; never linked into Switch builds.
namespace brls {
constexpr int FONT_INVALID = -1;
inline const std::string FONT_REGULAR = "regular";
inline const std::string FONT_CHINESE_SIMPLIFIED = "zh-Hans";
inline const std::string FONT_CHINESE_SIMPLIFIED_EXT = "zh-Hans-ext";
inline const std::string FONT_CHINESE_TRADITIONAL = "zh-Hant";
inline const std::string FONT_KOREAN_REGULAR = "ko";
inline const std::string FONT_SWITCH_ICONS = "switch-icons";
inline const std::string FONT_MATERIAL_ICONS = "material";

enum class Axis { ROW, COLUMN };
enum class AlignItems { AUTO, CENTER };
enum class ScrollingBehavior { NATURAL, CENTERED };
enum class ImageScalingType { FIT, FILL };

struct Style {
    float operator[](const std::string&) const { return 16.0f; }
};

struct Theme {
    NVGcolor operator[](const std::string&) const { return {}; }
};

struct FrameContext {};

struct ValueSelectedEvent {
    using Callback = std::function<void(int)>;
};

enum ControllerButton { BUTTON_A, BUTTON_B, BUTTON_START, BUTTON_LT, BUTTON_RT };
enum Sound { SOUND_NONE, SOUND_BACK };
enum class Visibility { VISIBLE, GONE };
enum class TransitionAnimation { NONE };

struct View;

struct Action {
    std::string label;
    std::function<bool(View*)> callback;
    bool hidden = false;
    bool repeating = false;
    Sound sound = SOUND_NONE;
};

struct View {
    virtual ~View() = default;
    View* parent = nullptr;
    View* defaultFocus = this;
    Visibility visibility = Visibility::VISIBLE;
    std::map<ControllerButton, Action> actions;
    bool focusable = false;
    bool actionsAvailable = true;
    float alpha = 1.0f;
    float width = 640.0f;
    float height = 480.0f;
    float shrink = 1.0f;
    float grow = 0.0f;
    bool hideHighlightBackground = false;
    std::function<bool(View*)> clickAction;

    View* getParent() { return parent; }
    virtual View* getDefaultFocus() {
        return (defaultFocus && defaultFocus != this) ? defaultFocus->getDefaultFocus() : this;
    }
    void setLastFocusedView(View* view) { defaultFocus = view ? view : this; }
    void setVisibility(Visibility value) { visibility = value; }
    void setFocusable(bool value) { focusable = value; }
    bool isFocusable() const { return focusable; }
    void setActionsAvailable(bool value) { actionsAvailable = value; }
    void setAlpha(float value) { alpha = value; }
    void setWidth(float value) { width = value; }
    void setHeight(float value) { height = value; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    void setWidthPercentage(float) {}
    void setDimensions(float w, float h) {
        width = w;
        height = h;
    }
    void setShrink(float value) { shrink = value; }
    void setGrow(float value) { grow = value; }
    void setMarginBottom(float) {}
    void setMarginRight(float) {}
    void setMargins(float, float, float, float) {}
    void setPadding(float, float, float, float) {}
    void setHideHighlightBackground(bool value) { hideHighlightBackground = value; }

    void registerAction(const std::string& label, ControllerButton button,
        const std::function<bool(View*)>& callback, bool hidden = false,
        bool repeating = false, Sound sound = SOUND_NONE) {
        actions.insert_or_assign(button, Action{label, callback, hidden, repeating, sound});
    }

    void registerClickAction(std::function<bool(View*)> callback) {
        clickAction = callback;
        registerAction("hints/ok", BUTTON_A, callback);
    }

    virtual void onFocusGained() {}
    virtual void onLayout() {}
    virtual void invalidate() { onLayout(); }
    virtual void freeView() { delete this; }
    virtual View* getView(const std::string&) { return nullptr; }
    virtual void willDisappear(bool) {}
    virtual void willAppear(bool) {}
    virtual void draw(NVGcontext*, float, float, float, float, Style, FrameContext*) {}

    NVGcolor a(NVGcolor c) const { return c; }
    NVGpaint a(NVGpaint p) const { return p; }
};

struct Box : View {
    std::vector<View*> children;

    explicit Box(Axis = Axis::ROW) {}
    ~Box() override { clearViews(true); }

    void setAlignItems(AlignItems) {}

    void addView(View* view) {
        if (!view)
            return;
        view->parent = this;
        children.push_back(view);
    }

    void clearViews(bool free = true) {
        if (free) {
            for (auto* child : children)
                delete child;
        } else {
            for (auto* child : children) {
                if (child)
                    child->parent = nullptr;
            }
        }
        children.clear();
        defaultFocus = this;
    }

    View* getDefaultFocus() override {
        if (defaultFocus && defaultFocus != this) {
            if (auto* target = defaultFocus->getDefaultFocus())
                return target;
        }
        if (focusable)
            return this;
        for (auto* child : children) {
            if (auto* target = child->getDefaultFocus())
                return target;
        }
        return nullptr;
    }

    void willAppear(bool resetState) override {
        for (auto* child : children) {
            if (child)
                child->willAppear(resetState);
        }
    }

    void willDisappear(bool resetState) override {
        for (auto* child : children) {
            if (child)
                child->willDisappear(resetState);
        }
    }
};

struct Label : View {
    std::string text;
    void setText(const std::string& value) { text = value; }
    const std::string& getFullText() const { return text; }
    void setSingleLine(bool) {}
    void setFontSize(float) {}
    void setLineHeight(float) {}
    void setTextColor(NVGcolor) {}
    View* getDefaultFocus() override { return nullptr; }
};

struct Header : View {
    std::string title;
    void setTitle(const std::string& value) { title = value; }
    View* getDefaultFocus() override { return nullptr; }
};

struct Button : View {
    Label label;
    Button() {
        setFocusable(true);
        label.parent = this;
    }
    void setText(const std::string& value) { label.setText(value); }
    const std::string& getText() const { return label.getFullText(); }
    void setFontSize(float) {}
    View* getDefaultFocus() override { return isFocusable() ? this : nullptr; }
    View* getView(const std::string& id) override {
        return id == "brls/button/label" ? &label : nullptr;
    }
};

struct DetailCell : View {
    Label titleLabel;
    Label detailLabel;
    Label* title = &titleLabel;
    Label* detail = &detailLabel;

    DetailCell() {
        setFocusable(true);
        titleLabel.parent = this;
        detailLabel.parent = this;
    }
    void setText(const std::string& value) { titleLabel.setText(value); }
    void setDetailText(const std::string& value) { detailLabel.setText(value); }
    void setTextColor(NVGcolor) {}
    void setDetailTextColor(NVGcolor) {}
    View* getDefaultFocus() override { return isFocusable() ? this : nullptr; }
};

struct Image : View {
    int texture = 0;
    int origW = 256;
    int origH = 256;

    void setFreeTexture(bool) {}
    void setScalingType(ImageScalingType) {}
    void clear() { texture = 0; origW = 0; origH = 0; }
    void setImageFromMem(const unsigned char* data, int size) {
        texture = (data && size > 0) ? 1 : 0;
        origW = texture ? 64 : 0;
        origH = texture ? 64 : 0;
    }
    inline void innerSetImage(int tex);
    int getTexture() const { return texture; }
    int getOriginalImageWidth() const { return origW; }
    int getOriginalImageHeight() const { return origH; }
    View* getDefaultFocus() override { return isFocusable() ? this : nullptr; }
};

struct SidebarItem : DetailCell {
    std::function<void(View*)> selectCallback;
    void onFocusGained() override {
        DetailCell::onFocusGained();
        if (selectCallback)
            selectCallback(this);
    }
};

struct Sidebar : Box {
    void addItem(const std::string& label, std::function<void(View*)> cb) {
        auto* item = new SidebarItem();
        item->setText(label);
        item->selectCallback = cb;
        item->registerClickAction([cb](View* v) {
            if (cb)
                cb(v);
            return true;
        });
        addView(item);
    }
    View* getItem(size_t index) { return children.at(index); }
};

struct ScrollingFrame : View {
    View* contentView = nullptr;
    float contentOffsetY = 0.0f;
    ScrollingBehavior behavior = ScrollingBehavior::NATURAL;

    ~ScrollingFrame() override { delete contentView; }

    void setContentView(View* view) {
        if (contentView && contentView != view)
            delete contentView;
        contentView = view;
        if (view) {
            view->parent = this;
            defaultFocus = view;
        }
    }
    View* getContentView() const { return contentView; }
    void setScrollingBehavior(ScrollingBehavior value) { behavior = value; }
    float getContentOffsetY() const { return contentOffsetY; }
    void setContentOffsetY(float value, bool) { contentOffsetY = value; }

    View* getDefaultFocus() override {
        return contentView ? contentView->getDefaultFocus() : nullptr;
    }
    void willAppear(bool resetState) override {
        if (contentView)
            contentView->willAppear(resetState);
    }
    void willDisappear(bool resetState) override {
        if (contentView)
            contentView->willDisappear(resetState);
    }
};

struct AppletFrame : View {
    View* contentView = nullptr;
    Box hintBox;
    std::string frameTitle;

    explicit AppletFrame(View* content) : contentView(content) {
        hintBox.parent = this;
        if (contentView) {
            contentView->parent = this;
            defaultFocus = contentView;
        }
    }
    ~AppletFrame() override { delete contentView; }

    View* getContentView() const { return contentView; }
    void setTitle(const std::string& title) { frameTitle = title; }
    View* getView(const std::string& id) override {
        return id == "brls/applet_frame/hint_box" ? &hintBox : nullptr;
    }
    View* getDefaultFocus() override {
        if (defaultFocus && defaultFocus != this) {
            if (auto* target = defaultFocus->getDefaultFocus())
                return target;
        }
        if (contentView) {
            if (auto* target = contentView->getDefaultFocus())
                return target;
        }
        return this;
    }
    void willAppear(bool resetState) override {
        if (contentView)
            contentView->willAppear(resetState);
    }
    void willDisappear(bool resetState) override {
        if (contentView)
            contentView->willDisappear(resetState);
    }
};

struct BottomBar : Box {
    std::map<std::string, View> widgets;
    BottomBar() {
        for (auto* id : {"brls/battery", "brls/wireless", "brls/hints/time"})
            widgets.try_emplace(id);
    }
    View* getView(const std::string& id) override { return &widgets.at(id); }
};

struct Activity {
    explicit Activity(View* content) : content(content) {}
    virtual ~Activity() {
        if (ownsContent)
            delete content;
    }
    View* content = nullptr;
    bool ownsContent = false;
    View* getContentView() { return content; }
    virtual void onPause() {}
    virtual void onResume() {}
};

struct Application {
    inline static std::vector<Activity*> activities;
    inline static View* currentFocus = nullptr;
    inline static std::map<std::string, std::function<View*()>> factories;
    inline static NVGcontext nvg;
    inline static std::vector<std::string> notifications;
    inline static std::vector<View*> focusHistory;
    inline static std::vector<std::unique_ptr<Activity>> ownedActivities;
    inline static std::map<std::string, int> fontStash;

    static int getFont(const std::string& name) {
        const auto it = fontStash.find(name);
        return it == fontStash.end() ? FONT_INVALID : it->second;
    }

    static bool loadFontFromMemory(const std::string& name, void* data, size_t size, bool) {
        if (!data || !size)
            return false;
        fontStash[name] = nvg.nextFont++;
        return true;
    }

    static std::vector<Activity*> getActivitiesStack() { return activities; }
    static View* getCurrentFocus() { return currentFocus; }
    static Style getStyle() { return {}; }
    static Theme getTheme() { return {}; }
    static NVGcontext* getNVGContext() { return &nvg; }
    static void notify(const std::string& message) { notifications.push_back(message); }

    static void giveFocus(View* view) {
        currentFocus = view ? view->getDefaultFocus() : nullptr;
        for (View* child = currentFocus; child && child->parent; child = child->parent)
            child->parent->setLastFocusedView(child);
        if (currentFocus)
            currentFocus->onFocusGained();
    }

    static void registerXMLView(const std::string& id, std::function<View*()> factory) {
        factories.insert_or_assign(id, std::move(factory));
    }

    static void pushActivity(Activity* activity, TransitionAnimation = TransitionAnimation::NONE) {
        if (!activities.empty()) {
            focusHistory.push_back(currentFocus);
            activities.back()->onPause();
        }
        activity->ownsContent = true;
        ownedActivities.emplace_back(activity);
        activities.push_back(activity);
        if (activity->getContentView())
            activity->getContentView()->willAppear(true);
        giveFocus(activity->getContentView());
    }

    static void popActivity(TransitionAnimation = TransitionAnimation::NONE) {
        if (activities.empty())
            return;
        Activity* top = activities.back();
        if (top->getContentView())
            top->getContentView()->willDisappear(false);
        activities.pop_back();
        if (!focusHistory.empty()) {
            currentFocus = focusHistory.back();
            focusHistory.pop_back();
        } else {
            currentFocus = nullptr;
        }
        auto it = std::find_if(ownedActivities.begin(), ownedActivities.end(),
            [top](const std::unique_ptr<Activity>& entry) { return entry.get() == top; });
        if (it != ownedActivities.end())
            ownedActivities.erase(it);
        if (!activities.empty())
            activities.back()->onResume();
    }

    static void clearActivities() {
        while (!activities.empty()) {
            if (ownedActivities.empty()) {
                activities.clear();
                focusHistory.clear();
                currentFocus = nullptr;
                break;
            }
            popActivity(TransitionAnimation::NONE);
        }
        ownedActivities.clear();
        notifications.clear();
        fontStash.clear();
        nvg = {};
    }
};

inline void Image::innerSetImage(int tex) {
    texture = tex;
    const auto it = Application::nvg.textureSizes.find(tex);
    if (it != Application::nvg.textureSizes.end()) {
        origW = it->second.first;
        origH = it->second.second;
    }
}

struct Dropdown : Box {
    std::string title;
    std::vector<std::string> values;
    ValueSelectedEvent::Callback selected;
    int current = 0;
    std::vector<DetailCell*> optionCells;

    Dropdown(std::string title, std::vector<std::string> values,
        ValueSelectedEvent::Callback selected, int current)
        : title(std::move(title))
        , values(std::move(values))
        , selected(std::move(selected))
        , current(current) {
        registerAction("hints/back", BUTTON_B, [](View*) {
            Application::popActivity(TransitionAnimation::NONE);
            return true;
        }, false, false, SOUND_BACK);
        for (size_t i = 0; i < this->values.size(); ++i) {
            auto* cell = new DetailCell();
            cell->setText(this->values[i]);
            auto cb = this->selected;
            const int idx = static_cast<int>(i);
            cell->registerClickAction([cb, idx](View*) {
                if (cb)
                    cb(idx);
                Application::popActivity(TransitionAnimation::NONE);
                return true;
            });
            optionCells.push_back(cell);
            addView(cell);
        }
        if (!optionCells.empty()) {
            const int clamped = (this->current >= 0 && this->current < static_cast<int>(optionCells.size()))
                ? this->current
                : 0;
            setLastFocusedView(optionCells[clamped]);
        }
    }
};

struct Dialog : Box {
    std::string message;
    AppletFrame frame{new Box(Axis::ROW)};
    std::vector<Button*> buttons;

    explicit Dialog(std::string text) : message(std::move(text)) {
        frame.parent = this;
        frame.registerAction("hints/back", BUTTON_B, [this](View*) {
            close();
            return true;
        }, false, false, SOUND_BACK);
    }

    void addButton(const std::string& label, std::function<void()> cb) {
        auto* btn = new Button();
        btn->setText(label);
        btn->registerClickAction([this, cb](View*) {
            close(cb);
            return true;
        });
        buttons.push_back(btn);
        static_cast<Box*>(frame.getContentView())->addView(btn);
        if (buttons.size() == 1) {
            frame.setLastFocusedView(btn);
            setLastFocusedView(&frame);
        }
    }

    AppletFrame* getAppletFrame() { return &frame; }

    void open() {
        Application::pushActivity(new Activity(this), TransitionAnimation::NONE);
    }

    void close(std::function<void()> after = {}) {
        if (!Application::activities.empty() &&
            Application::activities.back()->getContentView() == this) {
            Application::popActivity(TransitionAnimation::NONE);
        }
        if (after)
            after();
    }

    View* getDefaultFocus() override {
        return !buttons.empty() ? buttons.front() : static_cast<View*>(&frame);
    }
};

inline std::string getStr(const std::string& key) { return key; }
inline std::string getStr(const std::string& key, const std::string& arg) { return key + ":" + arg; }

namespace literals {
inline std::string operator""_i18n(const char* s, size_t n) { return std::string(s, n); }
} // namespace literals
} // namespace brls
