#pragma once
#include <JuceHeader.h>

// Пастельная палитра: чёрный фон, пастельный голубой, хаки и мягкие оттенки
namespace pal
{
    inline const juce::Colour bg       { 0xff101113 };
    inline const juce::Colour panel    { 0xff191b1e };
    inline const juce::Colour cell     { 0xff25282c };
    inline const juce::Colour cellAlt  { 0xff1e2024 };
    inline const juce::Colour edge     { 0xff33373d };
    inline const juce::Colour text     { 0xffe9e6dc };
    inline const juce::Colour textDim  { 0xff8f9094 };
    inline const juce::Colour onPastel { 0xff16171a };   // тёмный текст на пастельном фоне

    inline const juce::Colour blue  { 0xff9cc3e6 };
    inline const juce::Colour khaki { 0xffc8c08a };
    inline const juce::Colour sage  { 0xffa9c4a5 };
    inline const juce::Colour peach { 0xffe5b59f };
    inline const juce::Colour lilac { 0xffbdb5dc };
    inline const juce::Colour rose  { 0xffe2a7b8 };
    inline const juce::Colour teal  { 0xff9ed3c8 };
    inline const juce::Colour sand  { 0xffe3d7b4 };

    // цвет строки/пэда по номеру
    inline juce::Colour rowColour (int i)
    {
        static const juce::Colour colours[8] = { blue, khaki, sage, peach, lilac, rose, teal, sand };
        return colours[i & 7];
    }
}

class PastelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PastelLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, pal::bg);

        setColour (juce::Label::textColourId, pal::text);

        setColour (juce::Slider::trackColourId, pal::blue);
        setColour (juce::Slider::backgroundColourId, pal::edge);
        setColour (juce::Slider::thumbColourId, pal::sand);
        setColour (juce::Slider::textBoxTextColourId, pal::text);
        setColour (juce::Slider::textBoxBackgroundColourId, pal::panel);
        setColour (juce::Slider::textBoxOutlineColourId, pal::edge);

        setColour (juce::TextButton::buttonColourId, pal::cell);
        setColour (juce::TextButton::buttonOnColourId, pal::blue);
        setColour (juce::TextButton::textColourOffId, pal::text);
        setColour (juce::TextButton::textColourOnId, pal::onPastel);

        setColour (juce::ComboBox::backgroundColourId, pal::cell);
        setColour (juce::ComboBox::textColourId, pal::text);
        setColour (juce::ComboBox::outlineColourId, pal::edge);
        setColour (juce::ComboBox::arrowColourId, pal::khaki);

        setColour (juce::PopupMenu::backgroundColourId, pal::panel);
        setColour (juce::PopupMenu::textColourId, pal::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::blue);
        setColour (juce::PopupMenu::highlightedTextColourId, pal::onPastel);
    }
};
