#pragma once

#include <functional>
#include <vector>

namespace FocusNavigation {

enum class Command { None, Up, Down, Left, Right, Activate, Back };
enum class Axis { Vertical, Horizontal };

struct Input {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool activate = false;
    bool back = false;
};

Command Resolve(const Input& input, Axis axis);
int NextIndex(int current, int direction, int count, bool wrap,
              const std::function<bool(int)>& enabled = {});

class Path {
public:
    int Root() const { return nodes_.front(); }
    int Current() const { return nodes_.back(); }
    size_t Depth() const { return nodes_.size(); }
    void Focus(int index) { nodes_.back() = index; }
    void Enter(int index) { nodes_.push_back(index); }
    bool Back();
    void Reset(int root);

private:
    std::vector<int> nodes_{0};
};

class Frame {
public:
    void Reset() { consumed_ = false; }
    bool Consumed() const { return consumed_; }
    bool Consume();

private:
    bool consumed_ = false;
};

} // namespace FocusNavigation
