#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "NoteNames.h"
#include "MidiEngine.h"

namespace e4::ui {
inline const juce::Colour background{0xff101417}, panel{0xff191f24}, edge{0xff30393f};
inline const juce::Colour text{0xfff0f4ef}, muted{0xff97a49f}, accent{0xffc3f479};
inline const juce::Colour violet{0xffbdaaff}, danger{0xffffa28e};
inline juce::Font font(float size,bool bold = false) {
    return juce::Font(juce::FontOptions(size,bold ? juce::Font::bold : juce::Font::plain));
}
class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() {
        setColour(juce::Label::textColourId,text);
        setColour(juce::Slider::textBoxTextColourId,text);
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff11171b));
        setColour(juce::Slider::textBoxOutlineColourId,edge);
        setColour(juce::Slider::trackColourId,accent);
        setColour(juce::Slider::thumbColourId,accent);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff11171b));
        setColour(juce::ComboBox::textColourId,text);
        setColour(juce::ComboBox::outlineColourId,edge);
        setColour(juce::PopupMenu::backgroundColourId,panel);
        setColour(juce::PopupMenu::textColourId,text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,accent);
        setColour(juce::PopupMenu::highlightedTextColourId,background);
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff283139));
        setColour(juce::TextButton::textColourOffId,text);
        setColour(juce::ToggleButton::textColourId,text);
        setColour(juce::ToggleButton::tickColourId,accent);
        setColour(juce::ToggleButton::tickDisabledColourId,muted);
    }
    juce::Font getLabelFont(juce::Label&) override { return font(13); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(13); }
    juce::Font getTextButtonFont(juce::TextButton&,int) override { return font(13,true); }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool hover,bool down) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto role = button.getProperties()["role"].toString();
        auto colour = role == "primary" ? accent : role == "danger" ? juce::Colour(0xff392a29) : juce::Colour(0xff293239);
        if (role == "pad") {
            const int index = static_cast<int>(button.getProperties()["index"]);
            const std::array<juce::Colour,4> colours {accent,juce::Colour(0xff8edbd1),violet,juce::Colour(0xffe3be85)};
            const auto padColour = colours[static_cast<std::size_t>(juce::jlimit(0,3,index))];
            colour = down ? padColour : panel.interpolatedWith(padColour,hover ? 0.20f : 0.10f);
            g.setColour(colour); g.fillRoundedRectangle(bounds,10);
            g.setColour(padColour.withAlpha(hover || down ? 0.95f : 0.35f)); g.drawRoundedRectangle(bounds,10,1);
            g.setColour(down ? background : padColour);
            g.fillRoundedRectangle(bounds.getX()+14,bounds.getY()+12,18,3,1.5f);
            return;
        }
        if (!button.isEnabled()) colour = edge;
        if (hover) colour = colour.brighter(0.12f);
        if (down) colour = colour.darker(0.15f);
        const float opacity = button.isEnabled() ? 1.0f : 0.35f;
        g.setColour(colour.withMultipliedAlpha(opacity)); g.fillRoundedRectangle(bounds,8);
        g.setColour((role == "primary" && button.isEnabled() ? accent : edge).withMultipliedAlpha(opacity)); g.drawRoundedRectangle(bounds,8,1);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool down) override {
        const auto role = button.getProperties()["role"].toString();
        if (role == "pad") {
            auto bounds = button.getLocalBounds().reduced(14,10);
            g.setColour(down ? background : muted); g.setFont(font(10,true));
            g.drawText("0" + juce::String(static_cast<int>(button.getProperties()["index"])+1),bounds.removeFromTop(16),juce::Justification::right);
            g.setColour(down ? background : text); g.setFont(font(21,true));
            g.drawText(button.getButtonText(),bounds.removeFromTop(29),juce::Justification::left);
            g.setColour(down ? background.withAlpha(0.7f) : muted); g.setFont(font(10));
            g.drawText(button.getProperties()["detail"].toString(),bounds,juce::Justification::left);
            return;
        }
        g.setColour(button.isEnabled() ? (role == "primary" ? background : role == "danger" ? danger : text) : muted.withAlpha(0.6f));
        g.setFont(font(13,true)); g.drawFittedText(button.getButtonText(),button.getLocalBounds().reduced(8),juce::Justification::centred,1);
    }
    juce::Label* createSliderTextBox(juce::Slider& slider) override {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->getProperties().set("numeric",true);
        return label;
    }
    void drawLabel(juce::Graphics& g,juce::Label& label) override {
        if (!static_cast<bool>(label.getProperties()["numeric"]) || label.isBeingEdited()) {
            juce::LookAndFeel_V4::drawLabel(g,label); return;
        }
        auto bounds = label.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff11171b)); g.fillRoundedRectangle(bounds,7);
        g.setColour(edge); g.drawRoundedRectangle(bounds,7,1);
        g.setColour(text); g.setFont(font(13,true));
        g.drawFittedText(label.getText(),label.getLocalBounds().reduced(5,0),juce::Justification::centred,1);
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float,float,
                          juce::Slider::SliderStyle,juce::Slider& slider) override {
        const float centre = static_cast<float>(y)+static_cast<float>(height)*0.5f;
        const float left = static_cast<float>(x),right = static_cast<float>(x+width);
        g.setColour(edge); g.fillRoundedRectangle(left,centre-2,right-left,4,2);
        const float origin = slider.getMinimum() < 0 || static_cast<bool>(slider.getProperties()["bipolar"]) ? (left+right)*0.5f : left;
        g.setColour(accent); g.fillRoundedRectangle(juce::jmin(origin,position),centre-2,std::abs(position-origin),4,2);
        g.setColour(text); g.fillEllipse(position-5,centre-5,10,10);
    }
};
inline void label(juce::Label& control,bool secondary = false) {
    control.setColour(juce::Label::textColourId,secondary ? muted : text);
    control.setBorderSize(juce::BorderSize<int>(0));
}
class VoiceDisplay final : public juce::Component, public juce::SettableTooltipClient {
public:
    VoiceDisplay() { setTooltip("Four physical vocoder voices are shared by clips, keyboard and chord pads."); }
    void reset(int count,std::uint32_t serial) {
        voices = count; seen = serial; until = 0; warning = false; repaint();
    }
    void update(int count,std::uint32_t serial,e4::MidiEngine::Rejection reason) {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const bool newRejection = serial != seen;
        if (newRejection) {
            seen = serial; last = reason; until = now+2000;
            setTooltip(reason == e4::MidiEngine::Rejection::outOfRange
                ? "This transposed note is outside MIDI 0-127. Change the note or transpose."
                : "Four-voice limit reached. Fully release rejected keys before trying that pitch again.");
        }
        const bool nextWarning = now < until;
        if (voices != count || warning != nextWarning || newRejection) { voices = count; warning = nextWarning; repaint(); }
    }
    void paint(juce::Graphics& g) override {
        const auto colour = warning ? danger : accent;
        const float middle = static_cast<float>(getHeight())*0.5f;
        for (int i = 0; i < 4; ++i) {
            g.setColour(i < voices ? colour : edge);
            g.fillRoundedRectangle(static_cast<float>(i*12),middle-6,7,12,3);
        }
        g.setColour(warning ? danger : muted); g.setFont(font(10,true));
        const auto message = !warning ? "VOICES" : last == e4::MidiEngine::Rejection::outOfRange ? "OUT OF RANGE"
            : last == e4::MidiEngine::Rejection::counterOverflow ? "INPUT RESET" : "VOICE LIMIT";
        g.drawFittedText(message,56,0,juce::jmax(0,getWidth()-98),getHeight(),juce::Justification::centredLeft,1);
        g.setColour(text); g.setFont(font(13,true));
        g.drawText(juce::String(voices)+"/4",getWidth()-34,0,34,getHeight(),juce::Justification::right);
    }
private:
    int voices = 0;
    std::uint32_t seen = 0;
    e4::MidiEngine::Rejection last = e4::MidiEngine::Rejection::none;
    double until = 0;
    bool warning = false;
};
inline void rootStyle(juce::Slider& slider) {
    slider.setTextValueSuffix("");
    slider.textFromValueFunction = [](double value) { return juce::String(e4::noteLabel(juce::roundToInt(value))); };
    slider.valueFromTextFunction = [&slider](const juce::String& value) {
        const auto parsed = e4::parseNote(value.toStdString());
        return parsed ? static_cast<double>(*parsed) : slider.getValue();
    };
    slider.updateText();
    slider.setTooltip("Chord root: type a MIDI note number or name, e.g. 60, C4 or Db4. Middle C is C4 (60).");
}
inline void bendStyle(juce::Slider& slider) {
    slider.getProperties().set("bipolar",true);
    slider.textFromValueFunction = [](double value) {
        const int percent = juce::roundToInt((value-8192.0)/8192.0*100.0);
        return percent == 0 ? juce::String("Centre") : (percent > 0 ? "+" : "") + juce::String(percent) + "%";
    };
    slider.valueFromTextFunction = [](const juce::String& value) {
        if (value.containsIgnoreCase("centre") || value.containsIgnoreCase("center")) return 8192.0;
        return juce::jlimit(0.0,16383.0,8192.0+juce::jlimit(-100.0,100.0,value.getDoubleValue())*81.92);
    };
    slider.setTooltip("Pitch bend amount. Release to return to centre; the E-4 determines the musical range.");
}
inline void keyboardStyle(juce::MidiKeyboardComponent& keyboard) {
    keyboard.setOctaveForMiddleC(4);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xffdbe2db));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,juce::Colour(0xff10171a));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,panel);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,accent.withAlpha(0.8f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,accent.withAlpha(0.25f));
    keyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId,juce::Colour(0xff68776c));
}
inline void card(juce::Graphics& g,juce::Rectangle<float> bounds,const juce::String& heading) {
    g.setColour(panel); g.fillRoundedRectangle(bounds,14);
    g.setColour(edge.withAlpha(0.8f)); g.drawRoundedRectangle(bounds.reduced(0.5f),14,1);
    g.setColour(muted); g.setFont(font(11,true));
    g.drawText(heading,bounds.reduced(20,16).removeFromTop(16),juce::Justification::left);
}
inline void surface(juce::Graphics& g,int width,int height,bool standalone,bool connected) {
    const juce::Graphics::ScopedSaveState save(g);
    const float designWidth = standalone ? 1040.0f : 960.0f;
    const float designHeight = standalone ? 780.0f : 650.0f;
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(width)/designWidth,static_cast<float>(height)/designHeight));
    g.fillAll(background);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff25312a),0,0,background,designWidth*0.65f,200,false));
    g.fillRect(0.0f,0.0f,designWidth,92.0f);
    g.setColour(accent); g.fillRoundedRectangle(24,26,5,40,2);
    g.setColour(text); g.setFont(font(37,true)); g.drawText("E-4",42,18,95,48,juce::Justification::left);
    g.setColour(muted); g.setFont(font(11,true)); g.drawText("COMPANION",143,28,130,18,juce::Justification::left);
    g.setFont(font(11)); g.drawText(standalone ? "Standalone performance console" : "Hardware performance plug-in",143,47,240,16,juce::Justification::left);
    g.setColour(connected ? accent : muted); g.fillEllipse(designWidth-442,39,6,6);
    g.setFont(font(11,true)); g.drawText(standalone ? (connected ? "CONNECTED" : "DISCONNECTED") : "HOST ROUTING",static_cast<int>(designWidth)-426,31,170,22,juce::Justification::left);
    if (standalone) {
        card(g,{24,100,270,262},"01 / MIDI ROUTING");
        card(g,{24,376,270,294},"02 / CLOCK & TRANSPORT");
        card(g,{310,100,706,230},"PERFORMANCE");
        card(g,{310,344,706,152},"CHORD PALETTE");
        card(g,{310,510,706,160},"KEYBOARD");
        card(g,{24,684,992,72},"");
    } else {
        card(g,{24,100,912,210},"PERFORMANCE");
        card(g,{24,324,912,144},"CHORD PALETTE");
        card(g,{24,482,912,128},"KEYBOARD");
        g.setColour(muted); g.setFont(font(11)); g.drawText("MIDI TO YOUR HARDWARE. AUDIO FROM YOUR E-4.",24,621,912,16,juce::Justification::left);
    }
}
inline juce::Rectangle<int> bounds(int x,int y,int width,int height,int actualWidth,int actualHeight,bool standalone) {
    return juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height))
        .transformedBy(juce::AffineTransform::scale(static_cast<float>(actualWidth)/(standalone ? 1040.0f : 960.0f),static_cast<float>(actualHeight)/(standalone ? 780.0f : 650.0f))).toNearestInt();
}
}
