#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <functional>

// =============================================================================
//  Стиль: пастельный 8-бит аниме + VHS
// =============================================================================

namespace pal
{
    inline const juce::Colour ink      { 0xff5B4B8A };   // основной текст
    inline const juce::Colour inkSoft  { 0xff8C7BB5 };   // значения, подписи
    inline const juce::Colour pink     { 0xffFF9EC7 };
    inline const juce::Colour lavender { 0xffC9B3FF };
    inline const juce::Colour mint     { 0xff9EE8D4 };
    inline const juce::Colour sky      { 0xff9FD0FF };
    inline const juce::Colour lemon    { 0xffFFE69A };
    inline const juce::Colour peach    { 0xffFFC4A8 };
    inline const juce::Colour paper    { 0xffFFF7FB };   // фон панелей
    inline const juce::Colour track    { 0xffEADCF5 };   // пустая часть слайдера

    inline const juce::Colour bgTop    { 0xffFFD3E8 };
    inline const juce::Colour bgMid    { 0xffDCCBFF };
    inline const juce::Colour bgBot    { 0xffC6F1E6 };

    inline const juce::Colour glitchCyan { 0xff7FE7FF };
    inline const juce::Colour glitchPink { 0xffFF7FBF };

    // цвет строки секвенсора / пэда
    inline juce::Colour rowColour (int i)
    {
        static const juce::Colour colours[6] = { pink, lavender, mint, sky, lemon, peach };
        return colours[i % 6];
    }
}

// =============================================================================
//  Шрифты (вшиты через juce_add_binary_data, лицензии OFL лежат в Assets/)
// =============================================================================
namespace pixelfont
{
    inline juce::Typeface::Ptr vt323Face()
    {
        static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (
            BinaryData::VT323Regular_ttf, (size_t) BinaryData::VT323Regular_ttfSize);
        return f;
    }

    inline juce::Typeface::Ptr pressFace()
    {
        static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (
            BinaryData::PressStart2PRegular_ttf, (size_t) BinaryData::PressStart2PRegular_ttfSize);
        return f;
    }

    // основной текст: VT323 (размер 24-26)
    inline juce::Font vhs (float height)   { return juce::Font (juce::FontOptions (vt323Face()).withHeight (height)); }

    // заголовок (~30) и названия панелей (13): Press Start 2P (есть кириллица)
    inline juce::Font title (float height) { return juce::Font (juce::FontOptions (pressFace()).withHeight (height)); }
}

// =============================================================================
//  Пиксельная графика
// =============================================================================

// Прямоугольник со ступенчатыми (срезанными) углами
inline juce::Path steppedRect (juce::Rectangle<int> ri, int step = 3, int cutSteps = 2)
{
    const auto r = ri.toFloat();
    const float s = (float) step;
    const float c = s * (float) cutSteps;

    juce::Path p;
    float x = r.getX(), y = r.getY() + c;
    p.startNewSubPath (x, y);

    for (int i = 0; i < cutSteps; ++i) { x += s; p.lineTo (x, y); y -= s; p.lineTo (x, y); }   // левый верхний угол
    p.lineTo (r.getRight() - c, r.getY());

    x = r.getRight() - c; y = r.getY();
    for (int i = 0; i < cutSteps; ++i) { x += s; p.lineTo (x, y); y += s; p.lineTo (x, y); }   // правый верхний
    p.lineTo (r.getRight(), r.getBottom() - c);

    x = r.getRight(); y = r.getBottom() - c;
    for (int i = 0; i < cutSteps; ++i) { y += s; p.lineTo (x, y); x -= s; p.lineTo (x, y); }   // правый нижний
    p.lineTo (r.getX() + c, r.getBottom());

    x = r.getX() + c; y = r.getBottom();
    for (int i = 0; i < cutSteps; ++i) { x -= s; p.lineTo (x, y); y -= s; p.lineTo (x, y); }   // левый нижний
    p.closeSubPath();
    return p;
}

// Плашка: жёсткая тень, двойная рамка цвета панели, заливка paper (или своя)
inline void pixelBox (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour panel,
                      juce::Colour fill = pal::paper, int step = 3, int shadow = 4)
{
    if (shadow > 0)
    {
        g.setColour (pal::ink.withAlpha (0.18f));
        g.fillPath (steppedRect (r.translated (shadow, shadow), step, 2));
    }

    g.setColour (panel);
    g.fillPath (steppedRect (r, step, 2));

    g.setColour (fill);
    g.fillPath (steppedRect (r.reduced (step), step, 2));

    g.setColour (panel);
    g.strokePath (steppedRect (r.reduced (step * 2), step, 1), juce::PathStrokeType (1.0f));
}

// Спрайт: массив строк, '.' пусто, остальные символы окрашивает colourOf
using SpriteRows = const char* const*;

inline void drawSprite (juce::Graphics& g, SpriteRows rows, int numRows, int x, int y, int scale,
                        const std::function<juce::Colour (char)>& colourOf)
{
    for (int row = 0; row < numRows; ++row)
    {
        const char* line = rows[row];
        for (int col = 0; line[col] != 0; ++col)
        {
            if (line[col] == '.')
                continue;

            g.setColour (colourOf (line[col]));
            g.fillRect (x + col * scale, y + row * scale, scale, scale);
        }
    }
}

namespace sprites
{
    inline constexpr int cloudRows = 6;
    inline const char* const cloud[] = {
        "....####......",
        "..########.##.",
        ".############.",
        "##############",
        ".############.",
        "..##########.." };

    inline constexpr int sparkleRows = 5;
    inline const char* const sparkleA[] = {
        "..#..",
        ".###.",
        "#####",
        ".###.",
        "..#.." };
    inline const char* const sparkleB[] = {
        "..#..",
        "..#..",
        "#####",
        "..#..",
        "..#.." };

    inline constexpr int heartRows = 6;
    inline const char* const heart[] = {
        ".##.##.",
        "#######",
        "#######",
        ".#####.",
        "..###..",
        "...#..." };

    inline constexpr int miniHeartRows = 5;
    inline const char* const miniHeart[] = {
        ".#.#.",
        "#####",
        "#####",
        ".###.",
        "..#.." };

    // чиби-певица 12x14: H волосы, S кожа, E глаза, B румянец, m закрытый рот,
    // M открытый рот, D платье
    inline constexpr int chibiRows = 14;
    inline const char* const chibiClosed[] = {
        "...HHHHHH...",
        "..HHHHHHHH..",
        ".HHHHHHHHHH.",
        ".HHSSSSSSHH.",
        ".HSSSSSSSSH.",
        ".HSEESSEESH.",
        ".HSBSSSSBSH.",
        ".HSSSmmSSSH.",
        "..HSSSSSSH..",
        "..HDDDDDDH..",
        ".HDDDDDDDDH.",
        ".HDDDDDDDDH.",
        "...SS..SS...",
        "...SS..SS..." };
    inline const char* const chibiOpen[] = {
        "...HHHHHH...",
        "..HHHHHHHH..",
        ".HHHHHHHHHH.",
        ".HHSSSSSSHH.",
        ".HSSSSSSSSH.",
        ".HSEESSEESH.",
        ".HSBSSSSBSH.",
        ".HSSSMMSSSH.",
        "..HSSMMSSH..",
        "..HDDDDDDH..",
        ".HDDDDDDDDH.",
        ".HDDDDDDDDH.",
        "...SS..SS...",
        "...SS..SS..." };
}

// =============================================================================
//  LookAndFeel: сегментные слайдеры без ручек
// =============================================================================
class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr int segW = 6, segH = 14, segGap = 2;

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        juce::Slider::SliderLayout layout;
        layout.sliderBounds = s.getLocalBounds();
        return layout;
    }

    int getSliderThumbRadius (juce::Slider&) override { return 0; }

    static juce::Colour segmentColour (float t)
    {
        return t < 0.5f ? pal::pink.interpolatedWith (pal::lavender, t * 2.0f)
                        : pal::lavender.interpolatedWith (pal::sky, (t - 0.5f) * 2.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const int pitch = segW + segGap;
        const int n = juce::jmax (1, (width + segGap) / pitch);
        const int top = y + (height - segH) / 2;
        const double prop = juce::jlimit (0.0, 1.0, s.valueToProportionOfLength (s.getValue()));
        const int filled = (int) std::lround (prop * n);

        for (int i = 0; i < n; ++i)
        {
            const int sx = x + i * pitch;

            if (i < filled)
            {
                g.setColour (segmentColour (n > 1 ? (float) i / (float) (n - 1) : 0.0f));
                g.fillRect (sx, top, segW, segH);
                g.setColour (juce::Colours::white.withAlpha (0.55f));      // подсветка сверху
                g.fillRect (sx, top, segW, 2);
            }
            else
            {
                g.setColour (pal::track);
                g.fillRect (sx, top, segW, segH);
            }
        }

        // белый блок с рамкой ink на конце заполнения
        const int boundary = filled > 0 ? x + filled * pitch - segGap / 2 : x;
        const int mx = juce::jlimit (x, x + n * pitch - segGap - segW, boundary - segW / 2);
        g.setColour (juce::Colours::white);
        g.fillRect (mx, top - 2, segW, segH + 4);
        g.setColour (pal::ink);
        g.drawRect (mx, top - 2, segW, segH + 4, 1);
    }
};

// =============================================================================
//  Виджеты
// =============================================================================

// Строка параметра: название, сегментный слайдер, значение
class ParamRow : public juce::Component
{
public:
    static constexpr int nameW = 88, valueW = 78;

    ParamRow (const juce::String& n, std::function<juce::String (double)> fmt)
        : name (n), format (std::move (fmt))
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    ~ParamRow() override { attachment.reset(); }

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);   // двойной клик сбрасывает значение
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (pal::ink);
        g.setFont (pixelfont::vhs (24.0f));
        g.drawText (name, r.removeFromLeft (nameW), juce::Justification::centredLeft, false);

        g.setColour (pal::inkSoft);
        g.drawText (format (slider.getValue()), r.removeFromRight (valueW), juce::Justification::centredRight, false);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromLeft (nameW);
        r.removeFromRight (valueW);
        slider.setBounds (r.reduced (6, 0));
    }

    juce::Slider slider;

private:
    juce::String name;
    std::function<juce::String (double)> format;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Пиксельная кнопка (обычная или с фиксацией)
class PixelButton : public juce::Component
{
public:
    PixelButton (const juce::String& t, juce::Colour c, bool toggle = false)
        : text (t), colour (c), toggleMode (toggle) {}

    std::function<void()> onClick;

    void setOn (bool shouldBeOn)
    {
        if (on != shouldBeOn)
        {
            on = shouldBeOn;
            repaint();
        }
    }

    bool isOn() const { return on; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().withTrimmedRight (3).withTrimmedBottom (3);
        const int off = down ? 2 : 0;
        r = r.translated (off, off);

        pixelBox (g, r, colour, on ? colour : pal::paper, 2, down ? 0 : 3);

        g.setColour (pal::ink);
        g.setFont (pixelfont::vhs (24.0f));
        g.drawText (text, r, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false;

        if (getLocalBounds().contains (e.getPosition()))
        {
            if (toggleMode)
                on = ! on;

            if (onClick)
                onClick();
        }
        repaint();
    }

private:
    juce::String text;
    juce::Colour colour;
    bool toggleMode, on = false, down = false;
};

// Выбор из списка: клик по левой трети назад, иначе вперёд (колесо мыши тоже работает)
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (const juce::String& n, const juce::StringArray& displayLabels, juce::Colour c = pal::lavender)
        : name (n), labels (displayLabels), colour (c) {}

    ~ChoiceRow() override { attachment.reset(); }

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment.reset();
        param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID));

        if (param == nullptr)
            return;

        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            index = juce::roundToInt (v);
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (pal::ink);
        g.setFont (pixelfont::vhs (24.0f));
        g.drawText (name, r.removeFromLeft (ParamRow::nameW), juce::Justification::centredLeft, false);

        auto box = boxBounds();
        pixelBox (g, box, colour, pal::track, 2, 3);

        g.setColour (pal::ink);
        g.drawText (labels[juce::jlimit (0, juce::jmax (0, labels.size() - 1), index)], box, juce::Justification::centred, false);

        // стрелки
        g.setColour (pal::inkSoft);
        const int cy = box.getCentreY();
        for (int i = 0; i < 3; ++i)
        {
            g.fillRect (box.getX() + 10 + i, cy - 3 + i, 2, 7 - i * 2);              // <
            g.fillRect (box.getRight() - 12 - i, cy - 3 + i, 2, 7 - i * 2);          // >
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto box = boxBounds();
        step (e.x < box.getX() + box.getWidth() / 3 ? -1 : 1);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        step (w.deltaY > 0 ? -1 : 1);
    }

private:
    juce::Rectangle<int> boxBounds() const
    {
        return getLocalBounds().withTrimmedLeft (ParamRow::nameW + 6).withTrimmedRight (3).withTrimmedBottom (3).reduced (0, 1);
    }

    void step (int dir)
    {
        if (param == nullptr || attachment == nullptr)
            return;

        const int n = param->choices.size();
        index = (index + dir + n) % n;
        attachment->setValueAsCompleteGesture ((float) index);
        repaint();
    }

    juce::String name;
    juce::StringArray labels;
    juce::Colour colour;
    int index = 0;
    juce::AudioParameterChoice* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
