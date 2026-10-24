#pragma once
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
namespace e4 {
inline std::string noteLabel(int note) {
    if (note < 0 || note > 127) return {};
    constexpr std::array<const char*,12> names {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return std::string(names[static_cast<std::size_t>(note%12)]) + std::to_string(note/12-1)
        + " (" + std::to_string(note) + ")";
}
inline std::optional<int> parseNote(std::string_view text) {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    const auto number = [](std::string_view value) -> std::optional<int> {
        if (value.empty()) return {};
        int result = 0;
        const auto parsed = std::from_chars(value.data(),value.data()+value.size(),result);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size()) return {};
        return result;
    };
    if (const auto value = number(text)) return *value >= 0 && *value <= 127 ? value : std::nullopt;
    if (const auto bracket = text.find('('); bracket != std::string_view::npos) {
        // Accept our own formatted label only if the name and number agree.
        if (text.back() != ')') return {};
        const auto named = parseNote(text.substr(0,bracket));
        const auto numeric = number(text.substr(bracket+1,text.size()-bracket-2));
        return named && numeric && *named == *numeric ? named : std::nullopt;
    }
    if (text.size() < 2) return {};
    char letter = text.front();
    if (letter >= 'a' && letter <= 'g') letter = static_cast<char>(letter-'a'+'A');
    int pitch = -1;
    switch (letter) { case 'C': pitch=0; break; case 'D': pitch=2; break; case 'E': pitch=4; break;
        case 'F': pitch=5; break; case 'G': pitch=7; break; case 'A': pitch=9; break; case 'B': pitch=11; break; default: return {}; }
    text.remove_prefix(1);
    if (!text.empty() && (text.front() == '#' || text.front() == 'b')) {
        pitch += text.front() == '#' ? 1 : -1; text.remove_prefix(1);
    }
    const auto octave = number(text);
    if (!octave || *octave < -2 || *octave > 10) return {};
    const int note = (*octave+1)*12+pitch;
    return note >= 0 && note <= 127 ? std::optional<int>{note} : std::nullopt;
}
}
