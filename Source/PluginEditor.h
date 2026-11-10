/*
  ==============================================================================
    FreeTune - Editor con la interfaz "Gemini-Tune Pro"
    Reemplaza al PluginEditor.h original. El motor (PluginProcessor) no se toca.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Paleta y tipografia (sacadas del diseno HTML)
namespace gt
{
    const juce::Colour teal   { 0xff00f2c3 };
    const juce::Colour purple { 0xffa855f7 };
    const juce::Colour yellow { 0xffeab308 };
    const juce::Colour text   { 0xffe2e8f0 };   // slate-200
    const juce::Colour text2  { 0xffcbd5e1 };   // slate-300
    const juce::Colour muted  { 0xff94a3b8 };   // slate-400
    const juce::Colour faint  { 0xff64748b };   // slate-500
    const juce::Colour line   { 0xff1e293b };   // slate-800

    // Segoe UI y Consolas vienen con Windows; en otros sistemas cae a la fuente por defecto.
    juce::Font font (float size, bool bold = false, bool italic = false);
    juce::Font mono (float size, bool bold = false);
}

//==============================================================================
class GemLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GemLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
// Knob rotativo: el click derecho abre el menu del host (en FL: "Create automation clip")
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
// Pastilla con un puntito de color y un valor (35, 45, 30...)
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
// Linea de estado: "AutoKey Listen" con el punto que late + indicador de senal
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
// Nota grande + barra de cents con aguja
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
// Las 12 notas: fila de sostenidos (circulos) y fila de naturales (pastillas)
class NotePad : public juce::Component
{
public:
    std::function<bool (int)> isOn;       // pitch class (C = 0) -> esta activa?
    std::function<void (int)> onToggle;   // se hizo click en una nota

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> rectFor (int pitchClass) const;
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

    // Notas y escalas
    bool noteOn (int pitchClass) const;
    void setNote (int pitchClass, bool on);
    bool notesMatch (int key, int scaleType) const;
    void applyScale();
    int  noteMask() const;
    void resetNotes();
    void showKeyScaleMenu();
    juce::String keyScaleText() const;

    // Retune Speed (knob de "fuerza", 0..40) <-> parametro "speed" del motor (100 = lento, 0 = instantaneo)
    void strengthChanged();
    void syncStrengthFromParam();

    void toggleClassic();

    FreeTuneAudioProcessor& audioProcessor;
    GemLookAndFeel lnf;

    HostMenuSlider retuneKnob, humanizeKnob, flexKnob, detuneKnob;
    ValuePill retunePill, humanizePill, flexPill;
    StatusBar statusBar;
    ReadoutDisplay readout;
    NotePad notePad;

    std::unique_ptr<SliderAtt> humanizeAtt, flexAtt, detuneAtt;

    // Selectores ocultos: guardan Key y Scale en el proyecto y mantienen la logica original
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;

    bool strengthDragging = false;
    bool syncingStrength  = false;
    bool classicOn        = false;
    bool hqMode           = false;     // solo visual por ahora
    int  lastNoteMask     = -1;
    juce::String keyScaleLabel, detuneText;

    // Zonas clickeables / de repintado
    const juce::Rectangle<int> rangeRect     { 360, 24, 154, 44 };
    const juce::Rectangle<int> keyScaleRect  { 526, 24, 154, 44 };
    const juce::Rectangle<int> presetRect    { 696, 24, 320, 44 };
    const juce::Rectangle<int> resetRect     { 624, 330, 28, 24 };
    const juce::Rectangle<int> algoRect      { 712, 500, 284, 48 };
    const juce::Rectangle<int> lowLatRect    { 522, 520, 88, 28 };
    const juce::Rectangle<int> hqRect        { 610, 520, 40, 28 };
    const juce::Rectangle<int> detuneTextRect { 44, 512, 80, 40 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeTuneAudioProcessorEditor)
};
