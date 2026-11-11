/*
  ==============================================================================
    Autotune 2026 - Editor UI
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
namespace at26
{
    const juce::Colour bgOuter   { 0xff0d0f10 };
    const juce::Colour bgPanel   { 0xff141719 };
    const juce::Colour bgRight   { 0xff131517 };
    const juce::Colour bgDark    { 0xff0f1113 };
    const juce::Colour knobTop   { 0xff43474c };
    const juce::Colour knobBot   { 0xff323539 };
    const juce::Colour knobRing  { 0xff25282c };
    const juce::Colour line      { 0xff262a2e };

    const juce::Colour teal      { 0xff18e0b8 };
    const juce::Colour tealGlow  { 0xff0b2926 };
    const juce::Colour purple    { 0xff8b5cf6 };
    const juce::Colour lime      { 0xffa3e635 };
    const juce::Colour yellow    { 0xffeab308 };

    const juce::Colour text      { 0xfff1f5f9 };
    const juce::Colour text2     { 0xffcbd5e1 };
    const juce::Colour muted     { 0xff889098 };
    const juce::Colour faint     { 0xff4e545c };

    juce::Font font (float size, bool bold = false, bool italic = false);
    juce::Font mono (float size, bool bold = false);
}

//==============================================================================
class AT2026LookAndFeel : public juce::LookAndFeel_V4
{
public:
    AT2026LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
class HostMenuSlider : public juce::Slider
{
public:
    std::function<void (const juce::MouseEvent&)> onHostMenu;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

private:
    bool menuClick = false;
};

//==============================================================================
class ValuePill : public juce::Component
{
public:
    explicit ValuePill (juce::Colour dot) : dotColour (dot) {}
    void setText (const juce::String&);
    void paint (juce::Graphics&) override;

private:
    juce::Colour dotColour;
    juce::String text;
};

//==============================================================================
class StatusBar : public juce::Component
{
public:
    void setActive (bool);
    void tick();
    void paint (juce::Graphics&) override;

private:
    bool  active = false;
    float phase  = 0.0f;
};

//==============================================================================
class ReadoutDisplay : public juce::Component
{
public:
    void setReading (const juce::String& note, float cents, bool voiced);
    void paint (juce::Graphics&) override;

private:
    juce::String note { "--" };
    float cents = 0.0f;
    bool  voiced = false;
};

//==============================================================================
class NotePad : public juce::Component
{
public:
    std::function<bool (int)> isOn;
    std::function<void (int)> onToggle;

    void setActivePitchClass (int pc);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> rectFor (int pitchClass) const;
    int activePc = -1;
};

//==============================================================================
class FreeTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit FreeTuneAudioProcessorEditor (FreeTuneAudioProcessor&);
    ~FreeTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void styleKnob (HostMenuSlider&, juce::Colour arc, const juce::String& title);
    void attachDefault (HostMenuSlider&, const juce::String& paramId);
    void showHostMenu (const juce::String& paramId, const juce::MouseEvent&);

    bool noteOn (int pitchClass) const;
    void setNote (int pitchClass, bool on);
    bool notesMatch (int key, int scaleType) const;
    void applyScale();
    int  noteMask() const;
    void resetNotes();
    void showKeyScaleMenu();
    void showVocalRangeMenu();
    void showPresetMenu();
    void loadPreset (int index);
    bool isPresetDirty() const;
    juce::String keyScaleText() const;
    juce::String vocalRangeText() const;

    void strengthChanged();
    void syncStrengthFromParam();

    void toggleClassic();
    void toggleBypass();

    FreeTuneAudioProcessor& audioProcessor;
    AT2026LookAndFeel lnf;

    HostMenuSlider retuneKnob, humanizeKnob, flexKnob, detuneKnob;
    ValuePill retunePill, humanizePill, flexPill;
    StatusBar statusBar;
    ReadoutDisplay readout;
    NotePad notePad;

    std::unique_ptr<SliderAtt> humanizeAtt, flexAtt, detuneAtt;

    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;

    bool strengthDragging = false;
    bool syncingStrength  = false;
    bool classicOn        = false;
    bool hqMode           = false;
    bool bypassed         = false;
    int  lastNoteMask     = -1;
    int  currentPreset    = 0; // 0 = Max AutoTune, 1 = Natural
    bool lastPresetDirty  = false;
    juce::String keyScaleLabel, rangeLabel, detuneText;

    const juce::Rectangle<int> rangeRect      { 216, 12, 224, 68 };
    const juce::Rectangle<int> keyScaleRect   { 440, 12, 228, 68 };
    const juce::Rectangle<int> presetRect     { 680, 12, 348, 68 };
    const juce::Rectangle<int> presetPrevRect { 914, 28, 26, 36 };
    const juce::Rectangle<int> presetNextRect { 940, 28, 26, 36 };
    const juce::Rectangle<int> resetRect      { 616, 336, 28, 24 };
    const juce::Rectangle<int> algoRect       { 700, 502, 308, 44 };
    const juce::Rectangle<int> lowLatRect     { 360, 594, 108, 28 };
    const juce::Rectangle<int> hqRect         { 474, 594, 48, 28 };
    const juce::Rectangle<int> detuneTextRect { 36, 594, 86, 24 };
    const juce::Rectangle<int> powerBtnRect   { 964, 588, 38, 38 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeTuneAudioProcessorEditor)
};