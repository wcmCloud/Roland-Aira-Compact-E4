#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "MidiEngine.h"
#include "ChordPads.h"
#include "SettingsExchange.h"
#include "ControllerService.h"
#include "PerformanceQueue.h"
#include "InputNoteState.h"
#include <atomic>
class E4Processor final : public juce::AudioProcessor {
public:
    E4Processor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void requestPanic() noexcept { panicRequested.store(true); }
    // Message-thread producer only. Never call this from a host automation callback.
    bool postPerformance(e4::PerformanceCommand command) noexcept { return performance.push(command); }
    void requestUiRelease() noexcept { uiReleaseRequested.store(true); }
    int activeVoices() const noexcept { return voiceCount.load(); }
    std::uint32_t rejectionCount() const noexcept { return rejectedCount.load(); }
    e4::MidiEngine::Rejection lastRejection() const noexcept { return static_cast<e4::MidiEngine::Rejection>(rejectedReason.load()); }
    std::array<std::uint64_t,2> incomingNotes() const noexcept { return {inputLow.load(),inputHigh.load()}; }
    e4::ControllerService controllers;
    int effectiveValue(std::size_t index) const;
    e4::ScaleSettings scaleSettings() const;
    e4::PerformanceSettings chordBank() const;
    bool setChordBank(const e4::PerformanceSettings& bank);
    juce::AudioProcessorValueTreeState parameters;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    const juce::String getName() const override { return "E-4 Companion"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
private:
    e4::InputNoteState inputNotes;
    std::atomic<std::uint64_t> inputLow {0},inputHigh {0};
    e4::MidiEngine engine;
    juce::MidiBuffer output;
    e4::PerformanceQueue performance;
    e4::ChordPads chords;
    e4::ChordPads mappedChords {e4::source::firstMappedPad};
    e4::MappedParameters mappedValues;
    std::array<std::atomic<int>,5> mappedFeedback {},mappedBase {};
    std::atomic<bool> feedbackReady {false},resetMappedValues {false};
    e4::SettingsExchange bankExchange;
    std::uint64_t bankSerial = 0;
    std::atomic<bool> uiReleaseRequested { false };
    std::atomic<int> voiceCount { 0 };
    std::atomic<std::uint32_t> rejectedCount {0};
    std::atomic<int> rejectedReason {0};
    int lastRoot = 60;
    std::atomic<bool> panicRequested { false };
    bool wasPlaying = false;
    bool bypassed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(E4Processor)
};
