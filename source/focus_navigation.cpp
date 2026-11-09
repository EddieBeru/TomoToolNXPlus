#include "focus_navigation.h"

namespace FocusNavigation {

Command Resolve(const Input& input, Axis axis) {
    if (input.back) return Command::Back;
    if (input.activate) return Command::Activate;
    if (axis == Axis::Vertical) {
        if (input.up == input.down) return Command::None;
        return input.up ? Command::Up : Command::Down;
    }
    if (input.left == input.right) return Command::None;
    return input.left ? Command::Left : Command::Right;
}

int NextIndex(int current, int direction, int count, bool wrap,
              const std::function<bool(int)>& enabled) {
    if (count <= 0) return -1;
    const auto available = [&](int index) { return !enabled || enabled(index); };
    if (current < 0 || current >= count) {
        for (int i = 0; i < count; ++i) if (available(i)) return i;
        return -1;
    }
    if (direction != -1 && direction != 1) return available(current) ? current : -1;
    for (int step = 1; step <= count; ++step) {
        int candidate = current + direction * step;
        if (wrap) candidate = (candidate % count + count) % count;
        else if (candidate < 0 || candidate >= count) break;
        if (available(candidate)) return candidate;
    }
    return available(current) ? current : -1;
}

bool Path::Back() {
    if (nodes_.size() == 1) return false;
    nodes_.pop_back();
    return true;
}

void Path::Reset(int root) {
    nodes_.resize(1);
    nodes_.front() = root;
}

bool Frame::Consume() {
    if (consumed_) return false;
    consumed_ = true;
    return true;
}

} // namespace FocusNavigation
