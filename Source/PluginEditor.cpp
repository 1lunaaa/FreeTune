/*
  ==============================================================================
    FreeTune - Editor con la interfaz "Gemini-Tune Pro"

    Conectado hoy:
      - Botones de notas, Key & Scale y reset de notas
      - Retune Speed = fuerza del autotune (derecha = mas duro). Escribe el
        parametro "speed" del motor al reves: knob 40 = instantaneo, knob 0 = lento.
      - Humanize, Flex Tune, Detune (Concert A) y Modern/Classic
      - Nota detectada y barra de cents (en vivo)
    Solo decorativo por ahora: Vocal Range, Mode, preset, y los botones de abajo.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::Font gt::font (float size, bool bold, bool italic)
{
    int style = juce::Font::plain;
    if (bold)   style |= juce::Font::bold;
    if (italic) style |= juce::Font::italic;
    return juce::Font (juce::FontOptions ("Segoe UI", size, style));
}

juce::Font gt::mono (float size, bool bold)
{
    return juce::Font (juce::FontOptions ("Consolas", size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
namespace
{
    // Los parametros de nota empiezan en A; la UI trabaja en pitch classes (C = 0).
    const char* const kNoteParamIds[12] = { "noteA", "noteBb", "noteB", "noteC", "noteDb", "noteD",
                                            "noteEb", "noteE", "noteF", "noteGb", "noteG", "noteAb" };
    int paramIndexFor (int pitchClass) { return (pitchClass + 3) % 12; }

    bool isBlackKey (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    // Nombre con sostenido unicode (C♯, D♯...)
    juce::String pcName (int pc)
    {
        static const char* const names[12] = { "C", "C\xe2\x99\xaf", "D", "D\xe2\x99\xaf", "E", "F",
                                               "F\xe2\x99\xaf", "G", "G\xe2\x99\xaf", "A", "A\xe2\x99\xaf", "B" };
        return juce::String (juce::CharPointer_UTF8 (names[((pc % 12) + 12) % 12]));
    }

    // Nombre plano con # (para el menu de tonalidades)
    juce::String keyName (int pc)
    {
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((pc % 12) + 12) % 12]);
    }

    // type: 0 = mayor, 1 = menor, 2 = cromatica
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
        return (float) (100.0 * (1.0 - juce::jlimit (0.0, 40.0, k) / 40.0));
    }

    double speedToStrength (float speed)
    {
        return 40.0 * (1.0 - (double) juce::jlimit (0.0f, 100.0f, speed) / 100.0);
    }

    //----------------------------------------------------------------------------
    void drawText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                   juce::Justification j, const juce::Font& f, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (f);
        g.drawText (s, r, j, false);
    }

    // Panel hundido (el .panel-inset del CSS)
    void drawInsetPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e1015), 0.0f, r.getY(),
                                                 juce::Colour (0xff141720), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, radius);
            g.reduceClipRegion (clip);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, r.getY(),
                                                     juce::Colours::transparentBlack, 0.0f, r.getY() + 8.0f, false));
            g.fillRect (r);
        }

        g.setColour (juce::Colour (0xff1e2230));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    void strokeIcon (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c)
    {
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawChevron (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        juce::Path p;
        p.startNewSubPath (cx - 4.0f, cy - 2.0f);
        p.lineTo (cx, cy + 2.0f);
        p.lineTo (cx + 4.0f, cy - 2.0f);
        strokeIcon (g, p, 1.6f, c);
    }

    void drawArrow (juce::Graphics& g, float cx, float cy, bool right, juce::Colour c)
    {
        const float m = right ? 1.0f : -1.0f;
        juce::Path p;
        p.startNewSubPath (cx - 5.0f * m, cy);
        p.lineTo (cx + 5.0f * m, cy);
        p.startNewSubPath (cx + 1.0f * m, cy - 4.0f);
        p.lineTo (cx + 5.0f * m, cy);
        p.lineTo (cx + 1.0f * m, cy + 4.0f);
        strokeIcon (g, p, 1.5f, c);
    }

    void drawMenuIcon (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        juce::Path p;
        for (int i = -1; i <= 1; ++i)
        {
            p.startNewSubPath (cx - 6.0f, cy + (float) i * 4.0f);
            p.lineTo (cx + 6.0f, cy + (float) i * 4.0f);
        }
        strokeIcon (g, p, 1.5f, c);
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
        p.startNewSubPath (cx + 5.0f * m, cy + 4.0f);
        p.quadraticTo (cx + 6.0f * m, cy - 3.0f, cx - 2.0f * m, cy - 3.0f);
        p.startNewSubPath (cx - 1.0f * m, cy - 6.5f);
        p.lineTo (cx - 5.0f * m, cy - 3.0f);
        p.lineTo (cx - 1.0f * m, cy + 0.5f);
        strokeIcon (g, p, 1.6f, c);
    }

    void drawGear (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float pi = juce::MathConstants<float>::pi;
        const juce::Point<float> centre (cx, cy);
        juce::Path p;
        p.addEllipse (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
        p.addEllipse (cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * pi / 4.0f;
            p.startNewSubPath (centre.getPointOnCircumference (4.5f, a));
            p.lineTo (centre.getPointOnCircumference (7.0f, a));
        }
        strokeIcon (g, p, 1.5f, c);
    }

    void drawPower (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float pi = juce::MathConstants<float>::pi;
        juce::Path p;
        p.addCentredArc (cx, cy + 0.5f, 6.0f, 6.0f, 0.0f, 0.25f * pi, 1.75f * pi, true);
        p.startNewSubPath (cx, cy - 7.5f);
        p.lineTo (cx, cy - 1.0f);
        strokeIcon (g, p, 1.8f, c);
    }

    void drawMic (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float pi = juce::MathConstants<float>::pi;
        juce::Path p;
        p.addRoundedRectangle (cx - 2.5f, cy - 6.0f, 5.0f, 8.0f, 2.5f);
        p.startNewSubPath (cx + 5.0f, cy - 0.5f);
        p.addCentredArc (cx, cy - 0.5f, 5.0f, 5.0f, 0.0f, 0.5f * pi, 1.5f * pi, false);
        p.startNewSubPath (cx, cy + 4.5f);
        p.lineTo (cx, cy + 7.0f);
        strokeIcon (g, p, 1.3f, c);
    }

    //----------------------------------------------------------------------------
    void drawAlgoToggle (juce::Graphics& g, juce::Rectangle<float> r, bool classic)
    {
        g.setColour (juce::Colour (0x990f172a));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (0xcc1e293b));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        const float cy = r.getCentreY();

        // Modern (izquierda)
        const juce::Rectangle<float> dotL (r.getX() + 16.0f, cy - 3.0f, 6.0f, 6.0f);
        if (! classic)
        {
            juce::Path dp;
            dp.addEllipse (dotL);
            juce::DropShadow (gt::teal.withAlpha (0.8f), 6, {}).drawForPath (g, dp);
            g.setColour (gt::teal);
        }
        else
            g.setColour (juce::Colour (0xff334155));
        g.fillEllipse (dotL);
        drawText (g, "Modern", { r.getX() + 28.0f, r.getY(), 80.0f, r.getHeight() },
                  juce::Justification::centredLeft, gt::font (12.0f, true), classic ? gt::faint : juce::Colours::white);

        // Classic (derecha)
        const juce::Rectangle<float> dotR (r.getRight() - 22.0f, cy - 3.0f, 6.0f, 6.0f);
        if (classic)
        {
            juce::Path dp;
            dp.addEllipse (dotR);
            juce::DropShadow (gt::teal.withAlpha (0.8f), 6, {}).drawForPath (g, dp);
            g.setColour (gt::teal);
        }
        else
            g.setColour (juce::Colour (0xff334155));
        g.fillEllipse (dotR);
        drawText (g, "Classic", { r.getRight() - 108.0f, r.getY(), 80.0f, r.getHeight() },
                  juce::Justification::centredRight, gt::font (12.0f, true), classic ? juce::Colours::white : gt::faint);

        // Interruptor
        const juce::Rectangle<float> tr (r.getCentreX() - 24.0f, cy - 12.0f, 48.0f, 24.0f);
        g.setColour (juce::Colour (0xff0f1117));
        g.fillRoundedRectangle (tr, 12.0f);
        g.setColour (juce::Colour (0xff252a38));
        g.drawRoundedRectangle (tr.reduced (0.5f), 12.0f, 1.0f);

        const juce::Rectangle<float> knob (tr.getX() + (classic ? 26.0f : 2.0f), tr.getY() + 2.0f, 20.0f, 20.0f);
        juce::Path kp;
        kp.addEllipse (knob);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 4, { 0, 2 }).drawForPath (g, kp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4154), 0.0f, knob.getY(),
                                                 juce::Colour (0xff202533), 0.0f, knob.getBottom(), false));
        g.fillEllipse (knob);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRect (knob.getX() + 6.0f, knob.getY() + 1.0f, 8.0f, 1.0f);
    }
}

//==============================================================================
// Look and feel (knobs 3D con arco brillante)
//==============================================================================
GemLookAndFeel::GemLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141720));
    setColour (juce::PopupMenu::textColourId, gt::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0f766e));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Font GemLookAndFeel::getPopupMenuFont() { return gt::font (14.0f); }

void GemLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto  bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto  centre = bounds.getCentre();
    const float S      = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const bool  plain  = (bool) slider.getProperties()["plain"];
    const juce::Colour arcColour { (juce::uint32) (juce::int64) slider.getProperties()["arc"] };

    float bodyR = S * 0.5f - 1.0f;

    if (! plain)
    {
        const float ringR = S * 0.5f - 10.0f;
        const float track = 4.0f;
        bodyR = ringR - 7.0f;

        // Pista del arco
        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff1a1e29));
        g.strokePath (trackPath, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Arco de valor con brillo
        if (sliderPos > 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
            juce::Path stroked;
            juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (stroked, arc);
            juce::DropShadow (arcColour.withAlpha (0.6f), 7, {}).drawForPath (g, stroked);
            g.setColour (arcColour);
            g.fillPath (stroked);
        }
    }

    // Cuerpo del knob
    juce::Path body;
    body.addEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), juce::roundToInt (bodyR * 0.3f),
                      { 0, juce::roundToInt (bodyR * 0.12f) }).drawForPath (g, body);

    const juce::Point<float> hl (centre.x - bodyR * 0.3f, centre.y - bodyR * 0.3f);
    juce::ColourGradient grad (juce::Colour (0xff3a3f50), hl.x, hl.y,
                               juce::Colour (0xff151821), hl.x + bodyR * 1.5f, hl.y, true);
    grad.addColour (0.7, juce::Colour (0xff1e222d));
    g.setGradientFill (grad);
    g.fillPath (body);

    g.setColour (juce::Colour (0xff2c3244));
    g.strokePath (body, juce::PathStrokeType (1.0f));

    juce::Path shine;
    shine.addCentredArc (centre.x, centre.y, bodyR - 1.5f, bodyR - 1.5f, 0.0f, -0.7f, 0.7f, true);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (shine, juce::PathStrokeType (1.0f));

    // Indicador
    juce::Path pointer;
    pointer.startNewSubPath (centre.getPointOnCircumference (bodyR * (plain ? 0.2f : 0.32f), angle));
    pointer.lineTo (centre.getPointOnCircumference (bodyR * 0.82f, angle));
    g.setColour (juce::Colour (0xffcbd5e1));
    g.strokePath (pointer, juce::PathStrokeType (plain ? 2.0f : (S > 140.0f ? 4.0f : 3.0f),
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
// Pastilla de valor
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
    g.setColour (juce::Colour (0xff0f172a));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (gt::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (dotColour);
    g.fillEllipse (b.getX() + 9.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);

    drawText (g, text, b.withTrimmedLeft (18.0f).withTrimmedRight (6.0f),
              juce::Justification::centred, gt::mono (12.0f, true), juce::Colours::white);
}

//==============================================================================
// Linea de estado
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
    phase += 0.21f;
    repaint();
}

void StatusBar::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float pulse = 0.55f + 0.45f * std::sin (phase);

    const juce::Rectangle<float> dot (2.0f, h * 0.5f - 4.0f, 8.0f, 8.0f);
    juce::Path dp;
    dp.addEllipse (dot);
    juce::DropShadow (gt::teal.withAlpha (0.8f * pulse), 8, {}).drawForPath (g, dp);
    g.setColour (gt::teal.withAlpha (pulse));
    g.fillEllipse (dot);

    drawText (g, "AutoKey Listen", { 20.0f, 0.0f, 180.0f, h }, juce::Justification::centredLeft,
              gt::font (12.0f, true), gt::text2);

    // Indicador de senal (en el original era el boton "Enable Audio Demo")
    const juce::Rectangle<float> pill (w - 132.0f, 0.0f, 132.0f, h);
    g.setColour (active ? gt::teal.withAlpha (0.10f) : juce::Colour (0xff1e293b));
    g.fillRoundedRectangle (pill, 6.0f);
    g.setColour (active ? gt::teal : juce::Colour (0xff334155));
    g.drawRoundedRectangle (pill.reduced (0.5f), 6.0f, 1.0f);

    drawMic (g, pill.getX() + 17.0f, pill.getCentreY(), active ? juce::Colour (0xff5eead4) : gt::muted);
    drawText (g, active ? "Audio Active" : "No Signal", pill.withTrimmedLeft (30.0f),
              juce::Justification::centredLeft, gt::font (11.0f, true), active ? juce::Colour (0xff5eead4) : gt::muted);
}

//==============================================================================
// Nota grande y barra de cents
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

    // Nota grande
    const juce::Rectangle<float> noteArea (0.0f, 0.0f, w, 90.0f);
    g.setFont (gt::font (72.0f, true));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawText (note, noteArea.translated (0.0f, 3.0f), juce::Justification::centred, false);
    g.setColour (juce::Colours::white);
    g.drawText (note, noteArea, juce::Justification::centred, false);

    // Barra de cents
    const float barW = 448.0f, barH = 24.0f;
    const float bx = (w - barW) * 0.5f, by = 102.0f;
    const juce::Rectangle<float> bar (bx, by, barW, barH);

    g.setColour (juce::Colour (0xe60f172a));
    g.fillRoundedRectangle (bar, 6.0f);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bar, 6.0f);
        g.reduceClipRegion (clip);

        // Corchetes amarillos (zona de afinacion)
        g.setColour (gt::yellow.withAlpha (0.8f));
        g.fillRect (bx + barW * 0.25f, by + 4.0f, 2.0f, barH - 8.0f);
        g.fillRect (bx + barW * 0.75f - 2.0f, by + 4.0f, 2.0f, barH - 8.0f);

        // Aguja
        const float pct = voiced ? juce::jlimit (5.0f, 95.0f, 50.0f + cents * 0.5f) : 50.0f;
        const float nx  = bx + barW * pct / 100.0f;
        const float a   = voiced ? 1.0f : 0.35f;

        juce::Path glow;
        glow.addRectangle (nx - 1.0f, by, 2.0f, barH);
        juce::DropShadow (gt::teal.withAlpha (0.8f * a), 8, {}).drawForPath (g, glow);

        juce::ColourGradient ng (gt::teal.withAlpha (0.0f), nx - 4.0f, by,
                                 gt::teal.withAlpha (0.0f), nx + 4.0f, by, false);
        ng.addColour (0.5, gt::teal.withAlpha (a));
        g.setGradientFill (ng);
        g.fillRect (nx - 4.0f, by, 8.0f, barH);
    }

    g.setColour (juce::Colour (0xff1e293b));
    g.drawRoundedRectangle (bar.reduced (0.5f), 6.0f, 1.0f);

    // Regla
    static const char* const labels[5] = { "-100", "-50", "0", "+50", "+100" };
    const float ry = by + barH + 6.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float cx = bx + 4.0f + (barW - 8.0f) * (float) i / 4.0f;
        juce::Rectangle<float> r (cx - 24.0f, ry, 48.0f, 14.0f);
        juce::Justification j = juce::Justification::centred;
        if (i == 0) { r.setX (bx + 4.0f);                    j = juce::Justification::centredLeft; }
        if (i == 4) { r.setX (bx + barW - 4.0f - 48.0f);     j = juce::Justification::centredRight; }
        drawText (g, labels[i], r, j, gt::mono (10.0f, i == 2), i == 2 ? gt::muted : gt::faint);
    }
}

//==============================================================================
// Teclas de notas
//==============================================================================
juce::Rectangle<float> NotePad::rectFor (int pc) const
{
    static const int sharpSlot[12]   = { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };
    static const int naturalIdx[12]  = {  0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const float w = (float) getWidth();

    if (sharpSlot[pc] >= 0)
    {
        const float total = 6.0f * 48.0f + 5.0f * 12.0f;
        const float x0 = (w - total) * 0.5f;
        return { x0 + (float) sharpSlot[pc] * 60.0f, 0.0f, 48.0f, 48.0f };
    }

    const float total = 7.0f * 44.0f + 6.0f * 8.0f;
    const float x0 = (w - total) * 0.5f;
    return { x0 + (float) naturalIdx[pc] * 52.0f, 56.0f, 44.0f, 64.0f };
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
        const bool  sharp = isBlackKey (pc);
        const bool  on    = isOn ? isOn (pc) : true;
        const auto  r     = rectFor (pc);
        const float rad   = sharp ? r.getWidth() * 0.5f : 16.0f;

        if (! on)
            g.beginTransparencyLayer (0.35f);

        juce::Path shape;
        shape.addRoundedRectangle (r, rad);

        if (on)
            juce::DropShadow (gt::teal.withAlpha (sharp ? 0.5f : 0.35f), sharp ? 15 : 12, {}).drawForPath (g, shape);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff151821), 0.0f, r.getY(),
                                                 juce::Colour (0xff1a1e2a), 0.0f, r.getBottom(), false));
        g.fillPath (shape);

        if (on && sharp)
        {
            g.setColour (gt::teal.withAlpha (0.15f));
            g.fillPath (shape);
        }

        g.setColour (on ? gt::teal : juce::Colour (0xff1a1e28));
        g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.5f);

        drawText (g, pcName (pc), r, juce::Justification::centred,
                  gt::font (sharp ? 14.0f : 12.0f, true), on ? gt::teal : juce::Colour (0xff4a5568));

        if (! on)
            g.endTransparencyLayer();
    }
}

//==============================================================================
// Editor
//==============================================================================
FreeTuneAudioProcessorEditor::FreeTuneAudioProcessorEditor (FreeTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      retunePill (gt::teal), humanizePill (gt::purple), flexPill (gt::yellow)
{
    setLookAndFeel (&lnf);
    auto& apvts = audioProcessor.apvts;

    // --- Retune Speed: knob de "fuerza" (0..40), derecha = autotune mas duro ---
    styleKnob (retuneKnob, gt::teal, "Retune speed");
    retuneKnob.setRange (0.0, 40.0);
    retuneKnob.setValue (speedToStrength (apvts.getRawParameterValue ("speed")->load()), juce::dontSendNotification);
    retuneKnob.setDoubleClickReturnValue (true, 40.0);
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

    // --- Humanize, Flex Tune y Detune: conectados directo a sus parametros ---
    styleKnob (humanizeKnob, gt::purple, "Humanize");
    styleKnob (flexKnob,     gt::yellow, "Flex tune");
    styleKnob (detuneKnob,   gt::muted,  "Detune");
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

    // --- Key y Scale: selectores ocultos (guardan la eleccion y reusan la logica original) ---
    juce::StringArray keys;
    for (int pc = 0; pc < 12; ++pc)
        keys.add (keyName (pc));
    keyBox.addItemList (keys, 1);
    scaleBox.addItemList ({ "Major", "Minor", "Chromatic", "Custom" }, 1);
    keyAtt   = std::make_unique<ComboAtt> (apvts, "key",       keyBox);
    scaleAtt = std::make_unique<ComboAtt> (apvts, "scaleType", scaleBox);

    // --- Notas ---
    notePad.isOn = [this] (int pc) { return noteOn (pc); };
    notePad.onToggle = [this] (int pc)
    {
        setNote (pc, ! noteOn (pc));
        if (! notesMatch (keyBox.getSelectedItemIndex(), scaleBox.getSelectedItemIndex()))
            scaleBox.setSelectedItemIndex (3, juce::sendNotificationSync);       // "Custom"
        lastNoteMask = noteMask();
        notePad.repaint();
    };
    addAndMakeVisible (notePad);

    // Proyectos viejos: que las etiquetas coincidan con las notas guardadas.
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

    // Se conectan al final, para que la sincronizacion inicial no pise las notas guardadas.
    keyBox.onChange   = [this] { applyScale(); };
    scaleBox.onChange = [this] { applyScale(); };

    classicOn      = apvts.getRawParameterValue ("classic")->load() > 0.5f;
    lastNoteMask   = noteMask();
    keyScaleLabel  = keyScaleText();
    detuneText     = juce::String (detuneKnob.getValue(), 1) + " Hz";

    setSize (1040, 660);
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

// Click derecho: menu propio del DAW para ese parametro (FL: "Create automation clip", etc.)
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
// Retune Speed <-> parametro "speed"
//==============================================================================
void FreeTuneAudioProcessorEditor::strengthChanged()
{
    if (syncingStrength)
        return;

    if (auto* param = audioProcessor.apvts.getParameter ("speed"))
    {
        const float norm = param->convertTo0to1 (strengthToSpeed (retuneKnob.getValue()));
        const bool ownGesture = ! strengthDragging;      // doble click: no hay arrastre en curso
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
// Notas y escalas
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
        return;                                   // "Custom": las notas quedan como estan
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
        scaleBox.setSelectedItemIndex (2, juce::sendNotificationSync);    // Custom -> Chromatic
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

//==============================================================================
void FreeTuneAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();

    if (keyScaleRect.contains (pt))      showKeyScaleMenu();
    else if (resetRect.contains (pt))    resetNotes();
    else if (algoRect.contains (pt))     toggleClassic();
    else if (lowLatRect.contains (pt))   { hqMode = false; repaint (lowLatRect.getUnion (hqRect).expanded (4)); }
    else if (hqRect.contains (pt))       { hqMode = true;  repaint (lowLatRect.getUnion (hqRect).expanded (4)); }
}

void FreeTuneAudioProcessorEditor::timerCallback()
{
    auto& p = audioProcessor;

    // Nota detectada y cents (en vivo, desde el motor)
    const int  target  = p.meterTargetMidi.load();
    const bool voiced  = p.meterVoiced.load() && target >= 0;
    const int  shiftSt = juce::roundToInt (p.apvts.getRawParameterValue ("shift")->load());
    const juce::String noteText = voiced ? pcName (((target + shiftSt) % 12 + 12) % 12) : juce::String ("--");
    readout.setReading (noteText, voiced ? p.meterOutputCents.load() : 0.0f, voiced);

    statusBar.setActive (voiced);
    statusBar.tick();

    // Knobs y pastillas
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

    // Notas cambiadas por automatizacion o un preset
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

    const bool c = p.apvts.getRawParameterValue ("classic")->load() > 0.5f;
    if (c != classicOn)
    {
        classicOn = c;
        repaint (algoRect);
    }
}

//==============================================================================
void FreeTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();

    // Tarjeta
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff181b24), 0.0f, 0.0f,
                                             juce::Colour (0xff12141a), w, h, false));
    g.fillAll();
    g.setColour (juce::Colour (0xff232733));
    g.drawRect (0.0f, 0.0f, w, h, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (1.0f, 1.0f, w - 2.0f, 1.0f);

    // --- Cabecera ---
    g.setColour (juce::Colours::white);
    g.setFont (gt::font (20.0f, true, true));
    g.drawText ("GEMINI-TUNE", 24, 24, 140, 44, juce::Justification::centredLeft, false);

    const juce::Rectangle<float> badge (166.0f, 37.0f, 34.0f, 18.0f);
    g.setColour (gt::teal.withAlpha (0.10f));
    g.fillRoundedRectangle (badge, 4.0f);
    g.setColour (gt::teal.withAlpha (0.30f));
    g.drawRoundedRectangle (badge.reduced (0.5f), 4.0f, 1.0f);
    drawText (g, "PRO", badge, juce::Justification::centred, gt::mono (10.0f), juce::Colour (0xff2dd4bf));

    auto dropdown = [&] (juce::Rectangle<int> r, const juce::String& caption, const juce::String& value)
    {
        const auto f = r.toFloat();
        drawInsetPanel (g, f, 8.0f);
        drawText (g, caption.toUpperCase(), { f.getX() + 12.0f, f.getY() + 6.0f, f.getWidth() - 24.0f, 12.0f },
                  juce::Justification::centredLeft, gt::font (10.0f), gt::muted);
        drawText (g, value, { f.getX() + 12.0f, f.getY() + 21.0f, f.getWidth() - 40.0f, 16.0f },
                  juce::Justification::centredLeft, gt::font (12.0f, true), gt::text);
        drawChevron (g, f.getRight() - 18.0f, f.getY() + 29.0f, gt::muted);
    };
    dropdown (rangeRect,    "Vocal Range", "Alto/Tenor");
    dropdown (keyScaleRect, "Key & Scale", keyScaleLabel);

    // Preset
    {
        const auto f = presetRect.toFloat();
        drawInsetPanel (g, f, 8.0f);
        drawText (g, "Default*", { f.getX() + 12.0f, f.getY() + 6.0f, 120.0f, 16.0f },
                  juce::Justification::centredLeft, gt::font (12.0f, true), juce::Colours::white);
        drawText (g, "Factory", { f.getX() + 12.0f, f.getY() + 23.0f, 120.0f, 13.0f },
                  juce::Justification::centredLeft, gt::font (10.0f), gt::muted);
        drawArrow (g, f.getRight() - 78.0f, f.getCentreY(), false, gt::muted);
        drawArrow (g, f.getRight() - 52.0f, f.getCentreY(), true, gt::muted);
        drawMenuIcon (g, f.getRight() - 22.0f, f.getCentreY(), gt::muted);
    }

    g.setColour (gt::line.withAlpha (0.8f));
    g.fillRect (24.0f, 88.0f, 992.0f, 1.0f);

    // --- Panel izquierdo ---
    const juce::Rectangle<float> leftPanel (24.0f, 108.0f, 648.0f, 528.0f);
    drawInsetPanel (g, leftPanel, 12.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.03f), leftPanel.getX(), leftPanel.getY(),
                                             juce::Colours::transparentWhite, leftPanel.getRight(), leftPanel.getBottom(), false));
    g.fillRoundedRectangle (leftPanel, 12.0f);

    drawText (g, "EDIT NOTES", { 44.0f, 332.0f, 200.0f, 18.0f }, juce::Justification::centredLeft,
              gt::font (12.0f, true), gt::muted);
    drawResetIcon (g, (float) resetRect.getCentreX(), (float) resetRect.getCentreY(), gt::faint);

    // Detune (valor) y Mode
    g.setColour (gt::line.withAlpha (0.8f));
    g.fillRect (44.0f, 502.0f, 608.0f, 1.0f);
    drawText (g, "DETUNE", { 44.0f, 514.0f, 80.0f, 14.0f }, juce::Justification::centredLeft,
              gt::font (10.0f, true), gt::muted);
    drawText (g, detuneText, { 44.0f, 529.0f, 82.0f, 18.0f }, juce::Justification::centredLeft,
              gt::font (12.0f, true), gt::text);

    drawText (g, "MODE", { 462.0f, 518.0f, 50.0f, 32.0f }, juce::Justification::centredRight,
              gt::font (10.0f, true), gt::muted);
    {
        const juce::Rectangle<float> box (520.0f, 518.0f, 132.0f, 32.0f);
        g.setColour (juce::Colour (0xff0f172a));
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (gt::line);
        g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.0f);

        auto segment = [&] (juce::Rectangle<int> r, const juce::String& label, bool active)
        {
            const auto f = r.toFloat();
            if (active)
            {
                g.setColour (juce::Colour (0xff1e293b));
                g.fillRoundedRectangle (f, 6.0f);
                g.setColour (juce::Colour (0xff334155));
                g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, 1.0f);
            }
            drawText (g, label, f, juce::Justification::centred, gt::font (12.0f, true),
                      active ? juce::Colours::white : gt::muted);
        };
        segment (lowLatRect, "Low Latency", ! hqMode);
        segment (hqRect, "HQ", hqMode);
    }

    // --- Panel derecho ---
    const juce::Rectangle<float> rightPanel (692.0f, 108.0f, 324.0f, 528.0f);
    drawInsetPanel (g, rightPanel, 12.0f);

    drawText (g, "Retune Speed", { 720.0f, 128.0f, 180.0f, 24.0f }, juce::Justification::centredLeft,
              gt::font (14.0f, true), gt::text);
    drawText (g, "Humanize", { 712.0f, 336.0f, 134.0f, 18.0f }, juce::Justification::centred,
              gt::font (12.0f, true), gt::text2);
    drawText (g, "Flex Tune", { 862.0f, 336.0f, 134.0f, 18.0f }, juce::Justification::centred,
              gt::font (12.0f, true), gt::text2);

    drawAlgoToggle (g, algoRect.toFloat(), classicOn);

    g.setColour (gt::line.withAlpha (0.8f));
    g.fillRect (712.0f, 564.0f, 284.0f, 1.0f);

    // Botones de abajo (decorativos)
    for (int i = 0; i < 5; ++i)
    {
        const juce::Rectangle<float> b (712.0f + (float) i * 62.0f, 572.0f, 36.0f, 36.0f);
        const bool power = (i == 4);
        const float cx = b.getCentreX(), cy = b.getCentreY();

        if (power)
        {
            juce::Path bp;
            bp.addEllipse (b);
            juce::DropShadow (gt::teal.withAlpha (0.2f), 10, {}).drawForPath (g, bp);
            g.setColour (gt::teal.withAlpha (0.2f));
            g.fillEllipse (b);
            g.setColour (gt::teal.withAlpha (0.4f));
            g.drawEllipse (b.reduced (0.5f), 1.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff0f172a));
            g.fillEllipse (b);
            g.setColour (gt::line);
            g.drawEllipse (b.reduced (0.5f), 1.0f);
        }

        const auto ic = power ? juce::Colour (0xff2dd4bf) : gt::muted;
        switch (i)
        {
            case 0: drawUndoRedo (g, cx, cy, false, ic); break;
            case 1: drawUndoRedo (g, cx, cy, true, ic);  break;
            case 2:
            {
                g.setColour (ic);
                g.drawEllipse (cx - 7.5f, cy - 7.5f, 15.0f, 15.0f, 1.5f);
                drawText (g, "?", { cx - 8.0f, cy - 8.0f, 16.0f, 16.0f }, juce::Justification::centred,
                          gt::font (11.0f, true), ic);
                break;
            }
            case 3: drawGear (g, cx, cy, ic);  break;
            default: drawPower (g, cx, cy, ic); break;
        }
    }
}

void FreeTuneAudioProcessorEditor::resized()
{
    // Panel izquierdo
    statusBar.setBounds (44, 128, 608, 24);
    readout.setBounds   (44, 158, 608, 150);
    notePad.setBounds   (44, 358, 608, 120);
    detuneKnob.setBounds (128, 518, 32, 32);

    // Panel derecho
    retunePill.setBounds   (928, 129, 60, 22);
    retuneKnob.setBounds   (768, 154, 172, 172);
    humanizeKnob.setBounds (725, 356, 108, 108);
    flexKnob.setBounds     (875, 356, 108, 108);
    humanizePill.setBounds (749, 468, 60, 22);
    flexPill.setBounds     (899, 468, 60, 22);
}
