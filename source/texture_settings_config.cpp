#include "texture_settings_config.h"

#include <array>
#include <cstdio>

namespace TextureSettings {
namespace {
using namespace TextureProcessor;
constexpr std::array<const char*, RowCount> keys = {
    "canvas_shape", "encoder", "bc1_mode", "fit_mode", "matte"};
constexpr std::array<std::array<const char*, 3>, RowCount - 1> tokens = {{
    {"square", "book", "tv"}, {"custom", "pca", nullptr},
    {"auto", "4color", "3color"}, {"cover", "contain", "fill"}}};

int KeyIndex(const std::string& key) {
    for (int i = 0; i < RowCount; ++i) if (key == keys[i]) return i;
    return -1;
}

std::string Value(const State& state, int index) {
    if (index != static_cast<int>(Row::Background))
        return tokens[index][ValueIndex(state, static_cast<Row>(index))];
    if (state.matte.a == 0) return "transparent";
    char hex[7];
    std::snprintf(hex, sizeof(hex), "%02x%02x%02x", state.matte.r, state.matte.g, state.matte.b);
    return hex;
}

int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool DecodeValue(State& state, int index, const std::string& value) {
    if (index < RowCount - 1) {
        int selected = -1;
        for (int i = 0; i < ValueCount(static_cast<Row>(index)); ++i)
            if (tokens[index][i] == value) selected = static_cast<int>(i);
        if (selected < 0) return false;
        // Config retains inactive values, e.g. BC1 mode while PCA is selected.
        switch (static_cast<Row>(index)) {
            case Row::Shape: state.canvasShape = static_cast<CanvasShape>(selected); break;
            case Row::Encoder: state.encoder = static_cast<Bc1Encoder>(selected); break;
            case Row::Bc1Mode: state.bc1Mode = static_cast<Bc1Mode>(selected); break;
            case Row::Fit: state.fitMode = static_cast<FitMode>(selected); break;
            default: return false;
        }
    } else if (value == "transparent") state.matte = {};
    else {
        if (value.size() != 6) return false;
        for (char c : value) if (Hex(c) < 0) return false;
        state.matte = {static_cast<uint8_t>(Hex(value[0]) * 16 + Hex(value[1])),
                       static_cast<uint8_t>(Hex(value[2]) * 16 + Hex(value[3])),
                       static_cast<uint8_t>(Hex(value[4]) * 16 + Hex(value[5])), 255};
    }
    return true;
}

struct Line {
    size_t start, end, equal;
    int index;
};

std::vector<Line> Lines(const std::string& text) {
    std::vector<Line> result;
    for (size_t start = 0; start < text.size();) {
        auto newline = text.find('\n', start);
        const size_t end = newline == std::string::npos ? text.size() : newline + 1;
        auto equal = text.find('=', start);
        int index = -1;
        if (equal < end) index = KeyIndex(text.substr(start, equal - start));
        result.push_back({start, end, equal, index});
        start = end;
    }
    return result;
}
} // namespace

bool DecodeConfig(const std::string& text, State& state) {
    if (text.find('\0') != std::string::npos) return false;
    State parsed;
    for (const auto& line : Lines(text)) {
        if (line.index < 0) continue;
        auto value = text.substr(line.equal + 1, line.end - line.equal - 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' '))
            value.pop_back();
        if (!DecodeValue(parsed, line.index, value)) return false;
    }
    state = parsed;
    return true;
}

bool PatchConfig(const std::string& original, const State& before,
                 const State& after, std::string& output) {
    if (!ValidPreferences(before) || !ValidPreferences(after)) return false;
    std::array<bool, RowCount> changed{}, found{};
    for (int i = 0; i < RowCount; ++i) changed[i] = Value(before, i) != Value(after, i);
    std::string patched;
    for (const auto& line : Lines(original)) {
        if (line.index < 0 || !changed[line.index]) {
            patched.append(original, line.start, line.end - line.start);
            continue;
        }
        found[line.index] = true;
        size_t end = line.end;
        if (end > line.start && original[end - 1] == '\n') --end;
        if (end > line.start && original[end - 1] == '\r') --end;
        patched.append(original, line.start, line.equal - line.start + 1);
        patched += Value(after, line.index);
        patched.append(original, end, line.end - end);
    }
    const std::string newline = original.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    for (int i = 0; i < RowCount; ++i) if (changed[i] && !found[i]) {
        if (!patched.empty() && patched.back() != '\n') patched += newline;
        patched += std::string(keys[i]) + "=" + Value(after, i) + newline;
    }
    output = patched;
    return true;
}
} // namespace TextureSettings
