#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Theme.h"

class DrumRackEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit DrumRackEditor (DrumRackProcessor&);
    ~DrumRackEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // для инструмента снимков интерфейса (без цикла событий)
    void prepareSnapshot (int pageIndex, int frame, int glitchFrames);

    static constexpr int windowW = 800;
    static constexpr int windowH = 660;

private:
    class PadButton;
    class StepGrid;
    friend class PadButton;
    friend class StepGrid;

    void timerCallback() override;
    void advanceFrame();
    void selectPad (int index);
    void browseForSample();
    void showPage (int newPage);
    void rebuildBackground();
    void rebuildScanlines();

    void paintHeader (juce::Graphics&);
    void paintPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour c);
    void paintPadInfo (juce::Graphics&);
    void paintEqualizer (juce::Graphics&);

    PixelLookAndFeel laf;   // объявлен первым: переживает все компоненты

    DrumRackProcessor& proc;
    int selected = 0;
    int page = 0;
    int frame = 0;

    // вкладки
    PixelButton padsTab { "PADS", pal::pink };
    PixelButton seqTab  { "SEQUENCER", pal::lavender };

    // страница PADS
    std::unique_ptr<PadButton> pads[DrumRackProcessor::numPads];
    int lastHit[DrumRackProcessor::numPads] {};

    PixelButton loadBtn  { "LOAD", pal::mint };
    PixelButton clearBtn { "CLEAR", pal::peach };
    PixelButton learnBtn { "LEARN", pal::sky, true };

    ParamRow volRow, panRow, pitchRow, startRow, endRow, chokeRow;
    ChoiceRow loopChoice, rateChoice, stepChoice;
    ParamRow loopStartRow, loopEndRow, fadeRow;

    // страница SEQUENCER
    ChoiceRow patternChoice;
    PixelButton seqOnBtn { "SEQ ON", pal::mint, true };
    PixelButton clearPatBtn { "CLEAR", pal::peach };
    ParamRow swingRow, lenRow;
    std::unique_ptr<StepGrid> grid;
    std::unique_ptr<juce::ParameterAttachment> seqOnAttachment;

    std::vector<juce::Component*> padsPage, seqPage;
    std::unique_ptr<juce::FileChooser> chooser;

    // области панелей (считаются в resized)
    juce::Rectangle<int> padsPanel, samplePanel, loopPanel, seqCtrlPanel, seqGridPanel;

    // кэш картинок
    juce::Image background, scanlines;

    // анимация
    juce::Random rng;
    float cloudX[3] { 40.0f, 330.0f, 560.0f };
    float eqHeights[48] {};
    int glitchFrames = 0;
    int framesToGlitch = 55;
    int glitchY = 40, glitchH = 18;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackEditor)
};
