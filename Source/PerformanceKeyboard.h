#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cstdint>
namespace e4::ui {
// External highlights are painted independently of MidiKeyboardState. Updating
// them cannot call note listeners, retrigger output or release a mouse-held key.
class PerformanceKeyboard final : public juce::MidiKeyboardComponent {
public:
    PerformanceKeyboard(juce::MidiKeyboardState& state,Orientation orientation)
        : MidiKeyboardComponent(state,orientation) {}
    void showIncoming(std::uint64_t low,std::uint64_t high) {
        const std::array<std::uint64_t,2> next {low,high};
        if (next == incoming) return;
        // Follow newly pressed keys outside the current 49-note view.
        for (int note = 0; note < 128; ++note) {
            const auto bit = std::uint64_t{1} << (note%64);
            const auto slot = static_cast<std::size_t>(note/64);
            if ((next[slot] & bit) != 0 && (incoming[slot] & bit) == 0
                && (note < first || note > first+48)) {
                first = juce::jlimit(0,79,(note/12)*12-24);
                setAvailableRange(first,first+48);
                setLowestVisibleKey(first);
            }
        }
        incoming = next;
        repaint();
    }
    bool incomingDown(int note) const noexcept { return note >= 0 && note < 128
        && (incoming[static_cast<std::size_t>(note/64)] & (std::uint64_t{1} << (note%64))) != 0; }
protected:
    void drawWhiteNote(int note,juce::Graphics& g,juce::Rectangle<float> area,bool down,bool over,
                       juce::Colour lineColour,juce::Colour labelColour) override {
        MidiKeyboardComponent::drawWhiteNote(note,g,area,down || incomingDown(note),over,lineColour,labelColour);
    }
    void drawBlackNote(int note,juce::Graphics& g,juce::Rectangle<float> area,bool down,bool over,juce::Colour colour) override {
        MidiKeyboardComponent::drawBlackNote(note,g,area,down || incomingDown(note),over,colour);
    }
private:
    std::array<std::uint64_t,2> incoming {};
    int first = 36;
};
}
