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
    void resized() override;

private:
    class PadComponent;
    class StepGrid;
    friend class PadComponent;
    friend class StepGrid;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void selectPad (int index);
    void browseForSample();
    void showPage (int page);
    void timerCallback() override;
    void setupSlider (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix);
    void setupCombo (juce::ComboBox& b, juce::Label& l, const juce::String& text, const juce::StringArray& items);
    void resetPadAttachments();

    PastelLookAndFeel laf;   // объявлен первым, чтобы пережить все компоненты

    DrumRackProcessor& proc;
    int selected = 0;
    int page = 0;

    juce::TextButton padsTab { "PADS" }, seqTab { "SEQUENCER" };
    juce::Label brand;

    // --- страница PADS ---
    std::unique_ptr<PadComponent> pads[DrumRackProcessor::numPads];
    int lastHit[DrumRackProcessor::numPads] {};

    juce::Label padTitle, noteInfo;
    juce::TextButton loadButton { "Load sample..." }, clearButton { "Clear" }, learnButton { "MIDI Learn" };

    juce::Slider volSlider, panSlider, pitchSlider, startSlider, endSlider, chokeSlider;
    juce::Label  volLabel, panLabel, pitchLabel, startLabel, endLabel, chokeLabel;

    juce::ComboBox loopBox, rateBox, stepBox;
    juce::Slider   loopStartSlider, loopEndSlider, fadeSlider;
    juce::Label    loopLabel, loopStartLabel, loopEndLabel, rateLabel, stepLabel, fadeLabel;

    std::unique_ptr<SliderAttachment> volA, panA, pitchA, startA, endA, chokeA,
                                      loopStartA, loopEndA, fadeA;
    std::unique_ptr<ComboAttachment>  loopA, rateA, stepA;

    // --- страница SEQUENCER ---
    juce::TextButton seqOnButton { "SEQ ON" }, clearPatternButton { "Clear pattern" };
    juce::ComboBox patternBox;
    juce::Slider swingSlider, lenSlider;
    juce::Label patternLabel, swingLabel, lenLabel;
    std::unique_ptr<StepGrid> grid;

    std::unique_ptr<ButtonAttachment> seqOnA;
    std::unique_ptr<ComboAttachment>  patternA;
    std::unique_ptr<SliderAttachment> swingA, lenA;

    std::vector<juce::Component*> padsPage, seqPage;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackEditor)
};
