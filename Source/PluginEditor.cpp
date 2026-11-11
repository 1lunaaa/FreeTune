/*
  ==============================================================================
    Autotune 2026 - Editor UI
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::Font at26::font (float size, bool bold, bool italic)
{
    int style = juce::Font::plain;
    if (bold)   style |= juce::Font::bold;
    if (italic) style |= juce::Font::italic;
    return juce::Font (juce::FontOptions ("Segoe UI", size, style));
}

juce::Font at26::mono (float size, bool bold)
{
    return juce::Font (juce::FontOptions ("Consolas", size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
namespace
{
    const char* const kNoteParamIds[12] = { "noteA", "noteBb", "noteB", "noteC", "noteDb", "noteD",
                                            "noteEb", "noteE", "noteF", "noteGb", "noteG", "noteAb" };
    int paramIndexFor (int pitchClass) { return (pitchClass + 3) % 12; }

    bool isBlackKey (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    juce::String pcName (int pc)
    {
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F",
                                               "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((pc % 12) + 12) % 12]);
    }

    juce::String keyName (int pc)
    {
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((pc % 12) + 12) % 12]);
    }

    void scaleNotes (int key, int type, bool out[12])
    {
        static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
        for (int i = 0; i < 12; ++i)
            out[i] = (type == 2);
        if (type == 0 || type == 1)
        {
            const int* intervals = (type == 0) ? major : minor;
            for (int i = 0; i < 7; ++i)
                out[(key + intervals[i]) % 12] = true;
        }
    }

    float strengthToSpeed (double k)
    {
        return (float) (100.0 - juce::jlimit (0.0, 100.0, k));
    }

    double speedToStrength (float speed)
    {
        return 100.0 - (double) juce::jlimit (0.0f, 100.0f, speed);
    }

    void drawText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                   juce::Justification j, const juce::Font& f, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (f);
        g.drawText (s, r, j, false);
    }

    void strokeIcon (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c)
    {
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawChevron (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        juce::Path p;
        p.startNewSubPath (cx - 4.5f, cy - 2.0f);
        p.lineTo (cx, cy + 2.5f);
        p.lineTo (cx + 4.5f, cy - 2.0f);
        strokeIcon (g, p, 1.4f, c);
    }

    void drawArrow (juce::Graphics& g, float cx, float cy, bool right, juce::Colour c)
    {
        const float m = right ? 1.0f : -1.0f;
        juce::Path p;
        p.startNewSubPath (cx - 5.5f * m, cy);
        p.lineTo (cx + 5.5f * m, cy);
        p.startNewSubPath (cx + 1.5f * m, cy - 4.0f);
        p.lineTo (cx + 5.5f * m, cy);
        p.lineTo (cx + 1.5f * m, cy + 4.0f);
        strokeIcon (g, p, 1.4f, c);
    }

    void drawMenuIcon (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        juce::Path p;
        for (int i = -1; i <= 1; ++i)
        {
            p.startNewSubPath (cx - 6.5f, cy + (float) i * 4.0f);
            p.lineTo (cx + 6.5f, cy + (float) i * 4.0f);
        }
        strokeIcon (g, p, 1.4f, c);
    }

    void drawResetIcon (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float pi = juce::MathConstants<float>::pi;
        const juce::Point<float> centre (cx, cy);
        juce::Path p;
        p.addCentredArc (cx, cy, 5.5f, 5.5f, 0.0f, 0.25f * pi, 1.85f * pi, true);
        const auto tip = centre.getPointOnCircumference (5.5f, 1.85f * pi);
        p.startNewSubPath (tip.x - 1.0f, tip.y - 4.0f);
        p.lineTo (tip.x - 1.0f, tip.y);
        p.lineTo (tip.x + 4.0f, tip.y + 0.5f);
        strokeIcon (g, p, 1.5f, c);
    }

    void drawUndoRedo (juce::Graphics& g, float cx, float cy, bool redo, juce::Colour c)
    {
        const float m = redo ? -1.0f : 1.0f;
        juce::Path p;
        p.startNewSubPath (cx + 4.5f * m, cy + 3.5f);
        p.quadraticTo (cx + 5.5f * m, cy - 3.0f, cx - 2.0f * m, cy - 3.0f);
        p.startNewSubPath (cx - 1.0f * m, cy - 6.0f);
        p.lineTo (cx - 4.5f * m, cy - 3.0f);
        p.lineTo (cx - 1.0f * m, cy + 0.5f);
        strokeIcon (g, p, 1.5f, c);
    }

    void drawGear (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float pi = juce::MathConstants<float>::pi;
        const juce::Point<float> centre (cx, cy);
        juce::Path p;
        p.addEllipse (cx - 4.2f, cy - 4.2f, 8.4f, 8.4f);
        p.addEllipse (cx - 1.7f, cy - 1.7f, 3.4f, 3.4f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * pi / 4.0f;
            p.startNewSubPath (centre.getPointOnCircumference (4.2f, a));
            p.lineTo (centre.getPointOnCircumference (6.6f, a));
        }
        strokeIcon (g, p, 1.4f, c);
    }

    void drawPower (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float pi = juce::MathConstants<float>::pi;
        juce::Path p;
        p.addCentredArc (cx, cy + 0.5f, 5.8f, 5.8f, 0.0f, 0.25f * pi, 1.75f * pi, true);
        p.startNewSubPath (cx, cy - 7.0f);
        p.lineTo (cx, cy - 1.0f);
        strokeIcon (g, p, 1.6f, c);
    }

    void drawAlgoToggle (juce::Graphics& g, juce::Rectangle<float> r, bool classic)
    {
        g.setColour (at26::bgDark);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (at26::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        const float cy = r.getCentreY();

        // Modern (izquierda)
        const juce::Rectangle<float> dotL (r.getX() + 18.0f, cy - 3.0f, 6.0f, 6.0f);
        if (! classic)
        {
            g.setColour (at26::teal.withAlpha (0.35f));
            g.fillEllipse (dotL.expanded (2.5f));
            g.setColour (at26::teal);
        }
        else
            g.setColour (at26::faint);
        g.fillEllipse (dotL);
        drawText (g, "Modern", { r.getX() + 30.0f, r.getY(), 80.0f, r.getHeight() },
                  juce::Justification::centredLeft, at26::font (12.0f, true), classic ? at26::muted : juce::Colours::white);

        // Classic (derecha)
        const juce::Rectangle<float> dotR (r.getRight() - 24.0f, cy - 3.0f, 6.0f, 6.0f);
        if (classic)
        {
            g.setColour (at26::teal.withAlpha (0.35f));
            g.fillEllipse (dotR.expanded (2.5f));
            g.setColour (at26::teal);
        }
        else
            g.setColour (at26::faint);
        g.fillEllipse (dotR);
        drawText (g, "Classic", { r.getRight() - 110.0f, r.getY(), 80.0f, r.getHeight() },
                  juce::Justification::centredRight, at26::font (12.0f, true), classic ? juce::Colours::white : at26::muted);

        // Switch central
        const juce::Rectangle<float> tr (r.getCentreX() - 22.0f, cy - 11.0f, 44.0f, 22.0f);
        g.setColour (juce::Colour (0xff23272b));
        g.fillRoundedRectangle (tr, 11.0f);

        const juce::Rectangle<float> knob (tr.getX() + (classic ? 24.0f : 2.0f), tr.getY() + 2.0f, 18.0f, 18.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff585e66), 0.0f, knob.getY(),
                                                 juce::Colour (0xff3c4046), 0.0f, knob.getBottom(), false));
        g.fillEllipse (knob);
    }
}

//==============================================================================
AT2026LookAndFeel::AT2026LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, at26::bgPanel);
    setColour (juce::PopupMenu::textColourId, at26::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0f766e));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Font AT2026LookAndFeel::getPopupMenuFont() { return at26::font (13.5f); }

void AT2026LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto  bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto  centre = bounds.getCentre();
    const float S      = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const bool  plain  = (bool) slider.getProperties()["plain"];
    const juce::Colour arcColour { (juce::uint32) (juce::int64) slider.getProperties()["arc"] };

    if (plain)
    {
        const float ringR = S * 0.5f - 2.0f;
        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (at26::knobRing);
        g.strokePath (trackPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
            g.setColour (at26::teal.withAlpha (0.85f));
            g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float bodyR = ringR - 4.0f;
        g.setColour (at26::bgDark);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (at26::line);
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.startNewSubPath (centre.getPointOnCircumference (bodyR * 0.25f, angle));
        pointer.lineTo (centre.getPointOnCircumference (bodyR * 0.85f, angle));
        g.setColour (at26::muted);
        g.strokePath (pointer, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    const float ringR = S * 0.5f - 6.0f;
    const float track = (S > 140.0f) ? 3.5f : 3.0f;
    const float bodyR = ringR - 10.0f;

    juce::Path trackPath;
    trackPath.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    g.setColour (at26::knobRing);
    g.strokePath (trackPath, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
        g.setColour (arcColour.withAlpha (0.20f));
        g.strokePath (arc, juce::PathStrokeType (track + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (arcColour);
        g.strokePath (arc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR + 3.0f, bodyR * 2.0f, bodyR * 2.0f);

    g.setGradientFill (juce::ColourGradient (at26::knobTop, centre.x, centre.y - bodyR,
                                             at26::knobBot, centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    g.setColour (juce::Colour (0xff4e5359));
    g.drawEllipse (centre.x - bodyR + 0.5f, centre.y - bodyR + 0.5f, bodyR * 2.0f - 1.0f, bodyR * 2.0f - 1.0f, 1.0f);

    juce::Path pointer;
    pointer.startNewSubPath (centre.getPointOnCircumference (bodyR * 0.55f, angle));
    pointer.lineTo (centre.getPointOnCircumference (bodyR * 0.86f, angle));
    g.setColour (juce::Colour (0xffd8dee6));
    g.strokePath (pointer, juce::PathStrokeType ((S > 140.0f ? 2.6f : 2.0f),
                                                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void HostMenuSlider::mouseDown (const juce::MouseEvent& e)
{
    menuClick = e.mods.isPopupMenu() && onHostMenu != nullptr;
    if (menuClick) { onHostMenu (e); return; }
    juce::Slider::mouseDown (e);
}

void HostMenuSlider::mouseDrag (const juce::MouseEvent& e) { if (! menuClick) juce::Slider::mouseDrag (e); }

void HostMenuSlider::mouseUp (const juce::MouseEvent& e)
{
    if (menuClick) { menuClick = false; return; }
    juce::Slider::mouseUp (e);
}

//==============================================================================
void ValuePill::setText (const juce::String& t)
{
    if (t == text)
        return;
    text = t;
    repaint();
}

void ValuePill::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (at26::bgDark);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (at26::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

    g.setColour (dotColour);
    g.fillEllipse (b.getX() + 11.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);

    drawText (g, text, b.withTrimmedLeft (20.0f).withTrimmedRight (6.0f),
              juce::Justification::centred, at26::font (12.5f, true), juce::Colours::white);
}

//==============================================================================
void StatusBar::setActive (bool a)
{
    if (a != active)
    {
        active = a;
        repaint();
    }
}

void StatusBar::tick()
{
    phase += 0.16f;
    repaint();
}

void StatusBar::paint (juce::Graphics& g)
{
    const float h = (float) getHeight();
    const float pulse = active ? (0.70f + 0.30f * std::sin (phase)) : 0.90f;

    const juce::Rectangle<float> dot (4.0f, h * 0.5f - 3.5f, 7.0f, 7.0f);
    g.setColour (at26::teal.withAlpha (0.30f * pulse));
    g.fillEllipse (dot.expanded (3.0f));
    g.setColour (at26::teal.withAlpha (pulse));
    g.fillEllipse (dot);

    drawText (g, "AutoKey Listen", { 18.0f, 0.0f, 180.0f, h }, juce::Justification::centredLeft,
              at26::font (12.5f, true), at26::text2);
}

//==============================================================================
void ReadoutDisplay::setReading (const juce::String& n, float c, bool v)
{
    if (n == note && v == voiced && std::abs (c - cents) < 0.5f)
        return;
    note = n;
    cents = c;
    voiced = v;
    repaint();
}

void ReadoutDisplay::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const float cx = w * 0.5f;

    if (voiced)
    {
        juce::ColourGradient noteGlow (at26::teal.withAlpha (0.20f), cx, 44.0f,
                                       juce::Colours::transparentBlack, cx + 135.0f, 44.0f, true);
        noteGlow.addColour (0.45, at26::teal.withAlpha (0.08f));
        g.setGradientFill (noteGlow);
        g.fillEllipse (cx - 135.0f, -20.0f, 270.0f, 130.0f);
    }

    const juce::Rectangle<float> noteArea (0.0f, -4.0f, w, 92.0f);
    g.setFont (at26::font (78.0f, false));
    g.setColour (juce::Colours::white);
    g.drawText (note, noteArea, juce::Justification::centred, false);

    const float barW = 468.0f, barH = 26.0f;
    const float bx = (w - barW) * 0.5f, by = 100.0f;
    const juce::Rectangle<float> bar (bx, by, barW, barH);

    g.setColour (juce::Colour (0xff1e2326));
    g.fillRect (bar);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (bar.toNearestInt());

        const float pct = voiced ? juce::jlimit (4.0f, 96.0f, 50.0f + cents * 0.5f) : 50.0f;
        const float nx  = bx + barW * pct / 100.0f;
        const float a   = voiced ? 1.0f : 0.70f;

        if (voiced)
        {
            juce::ColourGradient barGlow (at26::teal.withAlpha (0.24f), nx, by + barH * 0.5f,
                                          juce::Colours::transparentBlack, nx + 34.0f, by + barH * 0.5f, true);
            g.setGradientFill (barGlow);
            g.fillRect (nx - 34.0f, by, 68.0f, barH);
        }

        g.setColour (at26::teal.withAlpha (a));
        g.fillRect (nx - 5.0f, by, 10.0f, barH);
    }

    const float leftBracketX  = bx + barW * 0.25f;
    const float rightBracketX = bx + barW * 0.75f;
    g.setColour (at26::yellow);
    g.fillRect (leftBracketX, by - 1.0f, 2.0f, barH + 2.0f);
    g.fillRect (leftBracketX, by - 1.0f, 5.0f, 2.0f);
    g.fillRect (leftBracketX, by + barH - 1.0f, 5.0f, 2.0f);

    g.fillRect (rightBracketX - 2.0f, by - 1.0f, 2.0f, barH + 2.0f);
    g.fillRect (rightBracketX - 5.0f, by - 1.0f, 5.0f, 2.0f);
    g.fillRect (rightBracketX - 5.0f, by + barH - 1.0f, 5.0f, 2.0f);

    const float tickTop = by + barH + 10.0f;
    for (int i = 0; i <= 40; ++i)
    {
        const float tx = bx + (barW * (float) i / 40.0f);
        const bool isMajor = (i % 10 == 0);
        const float th = isMajor ? 10.0f : 5.0f;
        g.setColour (isMajor ? at26::muted : at26::faint);
        g.fillRect (tx - 0.5f, tickTop + (isMajor ? 0.0f : 5.0f), 1.0f, th);
    }

    static const char* const labels[5] = { "-100", "-50", "0", "+50", "+100" };
    const float ry = tickTop + 14.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float lx = bx + (barW * (float) i / 4.0f);
        juce::Rectangle<float> r (lx - 24.0f, ry, 48.0f, 14.0f);
        drawText (g, labels[i], r, juce::Justification::centred, at26::font (11.0f, false), at26::muted);
    }
}

//==============================================================================
void NotePad::setActivePitchClass (int pc)
{
    if (pc != activePc)
    {
        activePc = pc;
        repaint();
    }
}

juce::Rectangle<float> NotePad::rectFor (int pc) const
{
    static const int sharpSlot[12]   = { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };
    static const int naturalIdx[12]  = {  0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const float w = (float) getWidth();

    if (sharpSlot[pc] >= 0)
    {
        const float total = 6.0f * 50.0f + 5.0f * 14.0f;
        const float x0 = (w - total) * 0.5f;
        return { x0 + (float) sharpSlot[pc] * 64.0f, 0.0f, 50.0f, 50.0f };
    }

    const float total = 7.0f * 52.0f + 6.0f * 10.0f;
    const float x0 = (w - total) * 0.5f;
    return { x0 + (float) naturalIdx[pc] * 62.0f, 60.0f, 52.0f, 74.0f };
}

void NotePad::mouseDown (const juce::MouseEvent& e)
{
    for (int pc = 0; pc < 12; ++pc)
        if (rectFor (pc).contains (e.position))
        {
            if (onToggle)
                onToggle (pc);
            return;
        }
}

void NotePad::paint (juce::Graphics& g)
{
    for (int pc = 0; pc < 12; ++pc)
    {
        const bool  sharp   = isBlackKey (pc);
        const bool  inScale = isOn ? isOn (pc) : true;
        const bool  singing = (pc == activePc);
        const auto  r       = rectFor (pc);
        const float rad     = sharp ? r.getWidth() * 0.5f : 22.0f;

        juce::Path shape;
        shape.addRoundedRectangle (r, rad);

        if (singing)
        {
            g.setGradientFill (juce::ColourGradient (at26::teal.withAlpha (0.55f), 0.0f, r.getY(),
                                                     at26::teal.withAlpha (0.22f), 0.0f, r.getBottom(), false));
            g.fillPath (shape);

            g.setColour (at26::teal);
            g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.5f);

            drawText (g, pcName (pc), r, juce::Justification::centred,
                      at26::font (13.0f, true), juce::Colours::white);
        }
        else if (inScale)
        {
            g.setColour (juce::Colour (0xff192222));
            g.fillPath (shape);

            g.setColour (at26::teal.withAlpha (0.55f));
            g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.2f);

            drawText (g, pcName (pc), r, juce::Justification::centred,
                      at26::font (13.0f, true), at26::text);
        }
        else
        {
            g.setColour (juce::Colour (0xff1b1e21));
            g.fillPath (shape);

            g.setColour (juce::Colour (0xff2b2f33));
            g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.0f);

            drawText (g, pcName (pc), r, juce::Justification::centred,
                      at26::font (13.0f, true), at26::muted);
        }
    }
}

//==============================================================================
FreeTuneAudioProcessorEditor::FreeTuneAudioProcessorEditor (FreeTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      retunePill (at26::teal), humanizePill (at26::purple), flexPill (at26::lime)
{
    setLookAndFeel (&lnf);
    auto& apvts = audioProcessor.apvts;

    styleKnob (retuneKnob, at26::teal, "Retune Speed");
    retuneKnob.setRange (0.0, 100.0);
    retuneKnob.setValue (speedToStrength (apvts.getRawParameterValue ("speed")->load()), juce::dontSendNotification);
    retuneKnob.setDoubleClickReturnValue (true, 100.0);
    retuneKnob.onValueChange = [this] { strengthChanged(); };
    retuneKnob.onDragStart = [this]
    {
        strengthDragging = true;
        if (auto* param = audioProcessor.apvts.getParameter ("speed"))
            param->beginChangeGesture();
    };
    retuneKnob.onDragEnd = [this]
    {
        if (auto* param = audioProcessor.apvts.getParameter ("speed"))
            param->endChangeGesture();
        strengthDragging = false;
    };
    retuneKnob.onHostMenu = [this] (const juce::MouseEvent& e) { showHostMenu ("speed", e); };

    styleKnob (humanizeKnob, at26::purple, "Humanize");
    styleKnob (flexKnob,     at26::lime,   "Flex Tune");
    styleKnob (detuneKnob,   at26::teal,   "Detune");
    detuneKnob.getProperties().set ("plain", true);

    humanizeAtt = std::make_unique<SliderAtt> (apvts, "humanize", humanizeKnob);
    flexAtt     = std::make_unique<SliderAtt> (apvts, "flex",     flexKnob);
    detuneAtt   = std::make_unique<SliderAtt> (apvts, "tune",     detuneKnob);
    attachDefault (humanizeKnob, "humanize");
    attachDefault (flexKnob,     "flex");
    attachDefault (detuneKnob,   "tune");
    humanizeKnob.onHostMenu = [this] (const juce::MouseEvent& e) { showHostMenu ("humanize", e); };
    flexKnob.onHostMenu     = [this] (const juce::MouseEvent& e) { showHostMenu ("flex", e); };
    detuneKnob.onHostMenu   = [this] (const juce::MouseEvent& e) { showHostMenu ("tune", e); };

    addAndMakeVisible (retunePill);
    addAndMakeVisible (humanizePill);
    addAndMakeVisible (flexPill);
    addAndMakeVisible (statusBar);
    addAndMakeVisible (readout);

    juce::StringArray keys;
    for (int pc = 0; pc < 12; ++pc)
        keys.add (keyName (pc));
    keyBox.addItemList (keys, 1);
    scaleBox.addItemList ({ "Major", "Minor", "Chromatic", "Custom" }, 1);
    keyAtt   = std::make_unique<ComboAtt> (apvts, "key",       keyBox);
    scaleAtt = std::make_unique<ComboAtt> (apvts, "scaleType", scaleBox);

    notePad.isOn = [this] (int pc) { return noteOn (pc); };
    notePad.onToggle = [this] (int pc)
    {
        setNote (pc, ! noteOn (pc));
        if (! notesMatch (keyBox.getSelectedItemIndex(), scaleBox.getSelectedItemIndex()))
            scaleBox.setSelectedItemIndex (3, juce::sendNotificationSync);
        lastNoteMask = noteMask();
        notePad.repaint();
    };
    addAndMakeVisible (notePad);

    {
        const int k = keyBox.getSelectedItemIndex(), t = scaleBox.getSelectedItemIndex();
        if (t >= 0 && t <= 2 && ! notesMatch (k, t))
        {
            int foundKey = -1, foundType = -1;
            for (int type = 0; type <= 2 && foundKey < 0; ++type)
                for (int key = 0; key < 12; ++key)
                    if (notesMatch (key, type)) { foundKey = key; foundType = type; break; }

            if (foundKey >= 0)
            {
                keyBox.setSelectedItemIndex (foundKey, juce::sendNotificationSync);
                scaleBox.setSelectedItemIndex (foundType, juce::sendNotificationSync);
            }
            else
                scaleBox.setSelectedItemIndex (3, juce::sendNotificationSync);
        }
    }

    keyBox.onChange   = [this] { applyScale(); };
    scaleBox.onChange = [this] { applyScale(); };

    classicOn     = apvts.getRawParameterValue ("classic")->load() > 0.5f;
    lastNoteMask  = noteMask();
    keyScaleLabel = keyScaleText();
    rangeLabel    = vocalRangeText();
    detuneText    = juce::String (detuneKnob.getValue(), 1) + " Hz";

    const float sp = apvts.getRawParameterValue ("speed")->load();
    currentPreset = (sp > 20.0f) ? 1 : 0;
    lastPresetDirty = isPresetDirty();

    setSize (1040, 650);
    startTimerHz (30);
}

FreeTuneAudioProcessorEditor::~FreeTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FreeTuneAudioProcessorEditor::styleKnob (HostMenuSlider& s, juce::Colour arc, const juce::String& title)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity (200);
    s.setTitle (title);
    s.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    s.getProperties().set ("arc", (juce::int64) arc.getARGB());
    addAndMakeVisible (s);
}

void FreeTuneAudioProcessorEditor::attachDefault (HostMenuSlider& s, const juce::String& paramId)
{
    if (auto* param = audioProcessor.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void FreeTuneAudioProcessorEditor::showHostMenu (const juce::String& paramId, const juce::MouseEvent& e)
{
    auto* param = audioProcessor.apvts.getParameter (paramId);
    if (param == nullptr)
        return;

    if (auto* host = getHostContext())
    {
        if (auto menu = host->getContextMenuForParameter (param))
        {
            menu->showNativeMenu (e.getEventRelativeTo (this).getPosition());
            return;
        }
    }

    juce::PopupMenu fallback;
    fallback.addItem ("Reset to default", [param]
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    });
    fallback.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

//==============================================================================
// Presets ("Max AutoTune" y "Natural")
//==============================================================================
void FreeTuneAudioProcessorEditor::loadPreset (int index)
{
    currentPreset = juce::jlimit (0, 1, index);

    auto setParam = [this] (const juce::String& id, float realVal)
    {
        if (auto* param = audioProcessor.apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (realVal));
            param->endChangeGesture();
        }
    };

    if (currentPreset == 0)
    {
        // Max AutoTune: velocidad maxima, sin humanize ni flex
        setParam ("speed",    0.0f);
        setParam ("humanize", 0.0f);
        setParam ("flex",     0.0f);
        setParam ("classic",  0.0f);
    }
    else
    {
        // Natural: velocidad suave, humanize y flex tune activos
        setParam ("speed",    65.0f);
        setParam ("humanize", 40.0f);
        setParam ("flex",     0.45f);
        setParam ("classic",  0.0f);
    }

    syncStrengthFromParam();
    classicOn = false;
    lastPresetDirty = false;
    repaint (presetRect);
    repaint (algoRect);
}

bool FreeTuneAudioProcessorEditor::isPresetDirty() const
{
    const float sp  = audioProcessor.apvts.getRawParameterValue ("speed")->load();
    const float hum = audioProcessor.apvts.getRawParameterValue ("humanize")->load();
    const float flx = audioProcessor.apvts.getRawParameterValue ("flex")->load();

    if (currentPreset == 0)
        return std::abs (sp - 0.0f) > 1.0f || std::abs (hum - 0.0f) > 1.0f || std::abs (flx - 0.0f) > 0.02f;

    return std::abs (sp - 65.0f) > 1.0f || std::abs (hum - 40.0f) > 1.0f || std::abs (flx - 0.45f) > 0.02f;
}

void FreeTuneAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Max AutoTune", true, currentPreset == 0);
    menu.addItem (2, "Natural",      true, currentPreset == 1);

    juce::Component::SafePointer<FreeTuneAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (presetRect))
                            .withMinimumWidth (presetRect.getWidth()),
                        [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->loadPreset (result - 1);
    });
}

//==============================================================================
void FreeTuneAudioProcessorEditor::strengthChanged()
{
    if (syncingStrength)
        return;

    if (auto* param = audioProcessor.apvts.getParameter ("speed"))
    {
        const float norm = param->convertTo0to1 (strengthToSpeed (retuneKnob.getValue()));
        const bool ownGesture = ! strengthDragging;
        if (ownGesture) param->beginChangeGesture();
        param->setValueNotifyingHost (norm);
        if (ownGesture) param->endChangeGesture();
    }
}

void FreeTuneAudioProcessorEditor::syncStrengthFromParam()
{
    if (strengthDragging)
        return;

    const double k = speedToStrength (audioProcessor.apvts.getRawParameterValue ("speed")->load());
    if (std::abs (retuneKnob.getValue() - k) > 0.05)
    {
        syncingStrength = true;
        retuneKnob.setValue (k, juce::dontSendNotification);
        syncingStrength = false;
    }
}

//==============================================================================
bool FreeTuneAudioProcessorEditor::noteOn (int pitchClass) const
{
    return audioProcessor.apvts.getRawParameterValue (kNoteParamIds[paramIndexFor (pitchClass)])->load() > 0.5f;
}

void FreeTuneAudioProcessorEditor::setNote (int pitchClass, bool on)
{
    if (auto* param = audioProcessor.apvts.getParameter (kNoteParamIds[paramIndexFor (pitchClass)]))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (on ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
}

bool FreeTuneAudioProcessorEditor::notesMatch (int key, int scaleType) const
{
    if (scaleType < 0 || scaleType > 2)
        return false;
    bool want[12];
    scaleNotes (juce::jmax (0, key), scaleType, want);
    for (int pc = 0; pc < 12; ++pc)
        if (noteOn (pc) != want[pc])
            return false;
    return true;
}

void FreeTuneAudioProcessorEditor::applyScale()
{
    const int type = scaleBox.getSelectedItemIndex();
    if (type < 0 || type > 2)
        return;
    bool want[12];
    scaleNotes (juce::jmax (0, keyBox.getSelectedItemIndex()), type, want);
    for (int pc = 0; pc < 12; ++pc)
        if (noteOn (pc) != want[pc])
            setNote (pc, want[pc]);
    lastNoteMask = noteMask();
    notePad.repaint();
}

int FreeTuneAudioProcessorEditor::noteMask() const
{
    int mask = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (noteOn (pc)) mask |= (1 << pc);
    return mask;
}

void FreeTuneAudioProcessorEditor::resetNotes()
{
    if (scaleBox.getSelectedItemIndex() == 3)
        scaleBox.setSelectedItemIndex (2, juce::sendNotificationSync);
    else
        applyScale();
    lastNoteMask = noteMask();
    notePad.repaint();
}

juce::String FreeTuneAudioProcessorEditor::keyScaleText() const
{
    const int k = juce::jmax (0, keyBox.getSelectedItemIndex());
    const int t = scaleBox.getSelectedItemIndex();
    if (t == 0) return keyName (k) + " Major";
    if (t == 1) return keyName (k) + " Minor";
    if (t == 2) return "Chromatic";
    return "Custom";
}

juce::String FreeTuneAudioProcessorEditor::vocalRangeText() const
{
    static const char* const names[5] = { "Soprano", "Alto", "Alto/Tenor", "Baritone", "Bass" };
    const int ri = juce::jlimit (0, 4, (int) audioProcessor.apvts.getRawParameterValue ("range")->load());
    return names[ri];
}

void FreeTuneAudioProcessorEditor::showVocalRangeMenu()
{
    static const char* const names[5] = { "Soprano", "Alto", "Alto/Tenor", "Baritone", "Bass" };
    const int cur = juce::jlimit (0, 4, (int) audioProcessor.apvts.getRawParameterValue ("range")->load());

    juce::PopupMenu menu;
    for (int i = 0; i < 5; ++i)
        menu.addItem (i + 1, names[i], true, cur == i);

    juce::Component::SafePointer<FreeTuneAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (rangeRect))
                            .withMinimumWidth (rangeRect.getWidth()),
                        [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (auto* param = safe->audioProcessor.apvts.getParameter ("range"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) (result - 1)));
            param->endChangeGesture();
        }
        safe->rangeLabel = safe->vocalRangeText();
        safe->repaint (safe->rangeRect);
    });
}

void FreeTuneAudioProcessorEditor::showKeyScaleMenu()
{
    const int curKey  = keyBox.getSelectedItemIndex();
    const int curType = scaleBox.getSelectedItemIndex();

    juce::PopupMenu menu;
    menu.addItem (1000, "Chromatic", true, curType == 2);
    menu.addSeparator();
    for (int pc = 0; pc < 12; ++pc)
    {
        juce::PopupMenu sub;
        sub.addItem (100 + pc * 2,     "Major", true, curKey == pc && curType == 0);
        sub.addItem (100 + pc * 2 + 1, "Minor", true, curKey == pc && curType == 1);
        menu.addSubMenu (keyName (pc), sub);
    }

    juce::Component::SafePointer<FreeTuneAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (keyScaleRect))
                            .withMinimumWidth (keyScaleRect.getWidth()),
                        [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result == 1000)
        {
            safe->scaleBox.setSelectedItemIndex (2, juce::sendNotificationSync);
        }
        else if (result >= 100)
        {
            const int key  = (result - 100) / 2;
            const int type = (result - 100) % 2;
            safe->keyBox.setSelectedItemIndex (key, juce::sendNotificationSync);
            safe->scaleBox.setSelectedItemIndex (type, juce::sendNotificationSync);
        }
        safe->applyScale();
    });
}

void FreeTuneAudioProcessorEditor::toggleClassic()
{
    const bool now = audioProcessor.apvts.getRawParameterValue ("classic")->load() > 0.5f;
    if (auto* param = audioProcessor.apvts.getParameter ("classic"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (now ? 0.0f : 1.0f);
        param->endChangeGesture();
    }
    classicOn = ! now;
    repaint (algoRect);
}

void FreeTuneAudioProcessorEditor::toggleBypass()
{
    bypassed = ! bypassed;
    if (auto* param = audioProcessor.apvts.getParameter ("mix"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (bypassed ? 0.0f : 1.0f);
        param->endChangeGesture();
    }
    repaint();
}

//==============================================================================
void FreeTuneAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();

    if (rangeRect.contains (pt))          showVocalRangeMenu();
    else if (keyScaleRect.contains (pt))  showKeyScaleMenu();
    else if (presetPrevRect.contains(pt)) loadPreset (currentPreset == 0 ? 1 : 0);
    else if (presetNextRect.contains(pt)) loadPreset (currentPreset == 0 ? 1 : 0);
    else if (presetRect.contains (pt))    showPresetMenu();
    else if (resetRect.contains (pt))     resetNotes();
    else if (algoRect.contains (pt))      toggleClassic();
    else if (powerBtnRect.contains (pt))  toggleBypass();
    else if (lowLatRect.contains (pt))
    {
        hqMode = false;
        repaint (lowLatRect.getUnion (hqRect).expanded (4));
    }
    else if (hqRect.contains (pt))
    {
        hqMode = true;
        repaint (lowLatRect.getUnion (hqRect).expanded (4));
    }
}

void FreeTuneAudioProcessorEditor::timerCallback()
{
    auto& p = audioProcessor;

    const int  target  = p.meterTargetMidi.load();
    const bool voiced  = (! bypassed) && p.meterVoiced.load() && target >= 0;
    const int  shiftSt = juce::roundToInt (p.apvts.getRawParameterValue ("shift")->load());
    const int  activePc = voiced ? (((target + shiftSt) % 12 + 12) % 12) : -1;
    const juce::String noteText = (activePc >= 0) ? pcName (activePc) : juce::String ("--");

    readout.setReading (noteText, voiced ? p.meterOutputCents.load() : 0.0f, voiced);
    notePad.setActivePitchClass (activePc);

    statusBar.setActive (voiced);
    statusBar.tick();

    syncStrengthFromParam();
    retunePill.setText (juce::String (juce::roundToInt (retuneKnob.getValue())));
    humanizePill.setText (juce::String (juce::roundToInt (humanizeKnob.getValue())));
    flexPill.setText (juce::String (juce::roundToInt (flexKnob.getValue() * 100.0)));

    const auto dt = juce::String (detuneKnob.getValue(), 1) + " Hz";
    if (dt != detuneText)
    {
        detuneText = dt;
        repaint (detuneTextRect);
    }

    const int mask = noteMask();
    if (mask != lastNoteMask)
    {
        lastNoteMask = mask;
        notePad.repaint();
    }

    const auto ks = keyScaleText();
    if (ks != keyScaleLabel)
    {
        keyScaleLabel = ks;
        repaint (keyScaleRect);
    }

    const auto vr = vocalRangeText();
    if (vr != rangeLabel)
    {
        rangeLabel = vr;
        repaint (rangeRect);
    }

    const bool c = p.apvts.getRawParameterValue ("classic")->load() > 0.5f;
    if (c != classicOn)
    {
        classicOn = c;
        repaint (algoRect);
    }

    const bool dirty = isPresetDirty();
    if (dirty != lastPresetDirty)
    {
        lastPresetDirty = dirty;
        repaint (presetRect);
    }
}

//==============================================================================
void FreeTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();

    // Fondo exterior oscuro
    g.fillAll (at26::bgOuter);

    // Chasis principal
    const juce::Rectangle<float> chassis (12.0f, 12.0f, w - 24.0f, h - 24.0f);
    const float splitX = 680.0f;

    // 1) Panel derecho (carbon oscuro neutro)
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (chassis, 6.0f);
        g.reduceClipRegion (clip);

        g.setColour (at26::bgRight);
        g.fillRect (splitX, 12.0f, w - 12.0f - splitX, h - 24.0f);

        // 2) Panel izquierdo con el brillo ambiental celeste-verdoso oscuro
        const juce::Rectangle<float> leftArea (12.0f, 12.0f, splitX - 12.0f, h - 24.0f);
        g.setColour (at26::bgPanel);
        g.fillRect (leftArea);

        if (! bypassed)
        {
            juce::ColourGradient ambientGlow (at26::tealGlow.withAlpha (0.48f), 260.0f, 220.0f,
                                              juce::Colours::transparentBlack, 680.0f, 540.0f, true);
            ambientGlow.addColour (0.45, at26::tealGlow.withAlpha (0.22f));
            g.setGradientFill (ambientGlow);
            g.fillRect (leftArea);

            juce::ColourGradient topLeftGlow (at26::teal.withAlpha (0.05f), 90.0f, 115.0f,
                                              juce::Colours::transparentBlack, 420.0f, 340.0f, true);
            g.setGradientFill (topLeftGlow);
            g.fillRect (leftArea);
        }
    }

    // Borde exterior del chasis
    g.setColour (at26::line);
    g.drawRoundedRectangle (chassis.reduced (0.5f), 6.0f, 1.0f);

    // Divisor vertical principal
    g.setColour (at26::line);
    g.fillRect (splitX, 12.0f, 1.0f, h - 24.0f);

    // --- Cabecera izquierda ---
    g.setColour (juce::Colours::white);
    g.setFont (at26::font (25.0f, true));
    g.drawText ("AutoTune", 38, 22, 118, 48, juce::Justification::centredLeft, false);

    // Simbolo R dibujado limpio (sin problemas de encoding)
    g.setColour (at26::muted);
    g.drawEllipse (153.0f, 30.0f, 9.0f, 9.0f, 0.9f);
    g.setFont (at26::font (6.5f, true));
    g.drawText ("R", 153, 30, 9, 9, juce::Justification::centred, false);

    // Divisores verticales de la barra superior izquierda
    g.setColour (at26::line);
    g.fillRect (216.0f, 12.0f, 1.0f, 68.0f);
    g.fillRect (440.0f, 12.0f, 1.0f, 68.0f);

    auto drawHeaderDropdown = [&] (juce::Rectangle<int> r, const juce::String& caption, const juce::String& value)
    {
        const auto f = r.toFloat();
        drawText (g, caption, { f.getX() + 22.0f, f.getY() + 15.0f, f.getWidth() - 50.0f, 16.0f },
                  juce::Justification::centredLeft, at26::font (12.0f, false), at26::muted);
        drawText (g, value, { f.getX() + 22.0f, f.getY() + 34.0f, f.getWidth() - 50.0f, 20.0f },
                  juce::Justification::centredLeft, at26::font (14.5f, true), juce::Colours::white);
        drawChevron (g, f.getRight() - 22.0f, f.getCentreY(), at26::faint);
    };
    drawHeaderDropdown (rangeRect,    "Vocal Range", rangeLabel);
    drawHeaderDropdown (keyScaleRect, "Key & Scale", keyScaleLabel);

    // Linea horizontal bajo cabecera izquierda
    g.setColour (at26::line);
    g.fillRect (12.0f, 80.0f, splitX - 12.0f, 1.0f);

    // --- Cabecera derecha (Presets: "Max AutoTune" y "Natural") ---
    {
        const auto f = presetRect.toFloat();
        juce::String presetTitle = (currentPreset == 0) ? "Max AutoTune" : "Natural";
        if (lastPresetDirty)
            presetTitle += "*";

        drawText (g, presetTitle, { f.getX() + 22.0f, f.getY() + 15.0f, 200.0f, 18.0f },
                  juce::Justification::centredLeft, at26::font (14.5f, true), juce::Colours::white);
        drawText (g, "Factory", { f.getX() + 22.0f, f.getY() + 35.0f, 160.0f, 16.0f },
                  juce::Justification::centredLeft, at26::font (12.5f, false), at26::muted);
        drawArrow (g, f.getRight() - 78.0f, f.getCentreY(), false, at26::muted);
        drawArrow (g, f.getRight() - 52.0f, f.getCentreY(), true, at26::muted);
        drawMenuIcon (g, f.getRight() - 24.0f, f.getCentreY(), at26::faint);
    }

    // Linea horizontal bajo cabecera derecha
    g.setColour (at26::line);
    g.fillRect (splitX, 80.0f, w - 12.0f - splitX, 1.0f);

    // --- Seccion Edit Notes (izquierda) ---
    drawText (g, "Edit Notes", { 56.0f, 338.0f, 140.0f, 20.0f }, juce::Justification::centredLeft,
              at26::font (13.0f, true), at26::text2);
    g.setColour (at26::line);
    g.fillRect (136.0f, 348.0f, 468.0f, 1.0f);
    drawResetIcon (g, (float) resetRect.getCentreX(), (float) resetRect.getCentreY(), at26::faint);

    // --- Barra inferior izquierda (Detune + Mode) ---
    g.setColour (at26::line);
    g.fillRect (12.0f, 560.0f, splitX - 12.0f, 1.0f);
    g.fillRect (336.0f, 560.0f, 1.0f, h - 12.0f - 560.0f);

    drawText (g, "Detune", { 36.0f, 574.0f, 80.0f, 16.0f }, juce::Justification::centredLeft,
              at26::font (12.0f, false), at26::muted);
    drawText (g, detuneText, { 36.0f, 594.0f, 86.0f, 24.0f }, juce::Justification::centredLeft,
              at26::font (15.0f, true), juce::Colours::white);

    drawText (g, "Mode", { 360.0f, 574.0f, 80.0f, 16.0f }, juce::Justification::centredLeft,
              at26::font (12.0f, false), at26::muted);
    {
        auto segment = [&] (juce::Rectangle<int> r, const juce::String& label, bool active)
        {
            const auto f = r.toFloat();
            if (active)
            {
                g.setColour (juce::Colour (0xff26292d));
                g.fillRoundedRectangle (f, 14.0f);
                g.setColour (juce::Colour (0xff3a3e44));
                g.drawRoundedRectangle (f.reduced (0.5f), 14.0f, 1.0f);
            }
            drawText (g, label, f, juce::Justification::centred, at26::font (12.5f, true),
                      active ? juce::Colours::white : at26::muted);
        };
        segment (lowLatRect, "Low Latency", ! hqMode);
        segment (hqRect, "HQ", hqMode);
    }

    // --- Panel derecho ---
    drawText (g, "Retune Speed", { 704.0f, 102.0f, 180.0f, 26.0f }, juce::Justification::centredLeft,
              at26::font (17.0f, false), at26::text);
    drawText (g, "Humanize", { 700.0f, 342.0f, 140.0f, 20.0f }, juce::Justification::centred,
              at26::font (13.5f, true), at26::text2);
    drawText (g, "Flex Tune", { 864.0f, 342.0f, 140.0f, 20.0f }, juce::Justification::centred,
              at26::font (13.5f, true), at26::text2);

    drawAlgoToggle (g, algoRect.toFloat(), classicOn);

    // Linea inferior derecha
    g.setColour (at26::line);
    g.fillRect (splitX, 572.0f, w - 12.0f - splitX, 1.0f);

    // Botones circulares inferiores
    for (int i = 0; i < 5; ++i)
    {
        const juce::Rectangle<float> b (704.0f + (float) i * 65.0f, 588.0f, 38.0f, 38.0f);
        const bool power = (i == 4);
        const float cx = b.getCentreX(), cy = b.getCentreY();

        if (power && ! bypassed)
        {
            g.setGradientFill (juce::ColourGradient (at26::teal.withAlpha (0.45f), cx, b.getY(),
                                                     at26::teal.withAlpha (0.22f), cx, b.getBottom(), false));
            g.fillEllipse (b);
            g.setColour (at26::teal.withAlpha (0.85f));
            g.drawEllipse (b.reduced (0.5f), 1.2f);
        }
        else
        {
            g.setColour (at26::bgDark);
            g.fillEllipse (b);
            g.setColour (at26::line);
            g.drawEllipse (b.reduced (0.5f), 1.0f);
        }

        const auto ic = (power && ! bypassed) ? juce::Colours::white : at26::muted;
        switch (i)
        {
            case 0: drawUndoRedo (g, cx, cy, false, ic); break;
            case 1: drawUndoRedo (g, cx, cy, true, ic);  break;
            case 2:
            {
                g.setColour (ic);
                g.drawEllipse (cx - 7.5f, cy - 7.5f, 15.0f, 15.0f, 1.4f);
                drawText (g, "?", { cx - 8.0f, cy - 8.0f, 16.0f, 16.0f }, juce::Justification::centred,
                          at26::font (11.0f, true), ic);
                break;
            }
            case 3: drawGear (g, cx, cy, ic);  break;
            default: drawPower (g, cx, cy, ic); break;
        }
    }
}

void FreeTuneAudioProcessorEditor::resized()
{
    statusBar.setBounds (44, 102, 600, 24);
    readout.setBounds   (36, 134, 620, 175);
    notePad.setBounds   (46, 376, 600, 140);
    detuneKnob.setBounds (124, 584, 34, 34);

    retunePill.setBounds   (938, 102, 68, 26);
    retuneKnob.setBounds   (756, 142, 196, 196);
    humanizeKnob.setBounds (716, 366, 108, 108);
    flexKnob.setBounds     (880, 366, 108, 108);
    humanizePill.setBounds (736, 472, 68, 24);
    flexPill.setBounds     (900, 472, 68, 24);
}