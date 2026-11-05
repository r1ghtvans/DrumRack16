#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

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
    friend class PadComponent;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void selectPad (int index);
    void browseForSample();
    void timerCallback() override;
    void setupSlider (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix);
    void setupCombo (juce::ComboBox& b, juce::Label& l, const juce::String& text, const juce::StringArray& items);
    void resetAttachments();

    DrumRackProcessor& proc;
    int selected = 0;

    std::unique_ptr<PadComponent> pads[DrumRackProcessor::numPads];
    int lastHit[DrumRackProcessor::numPads] {};

    juce::Label padTitle, noteInfo;
    juce::TextButton loadButton { "Load sample..." }, clearButton { "Clear" }, learnButton { "MIDI Learn" };

    // колонка 1: звук
    juce::Slider volSlider, panSlider, pitchSlider, startSlider, endSlider, chokeSlider;
    juce::Label  volLabel, panLabel, pitchLabel, startLabel, endLabel, chokeLabel;

    // колонка 2: петля и ритм
    juce::ComboBox loopBox, rateBox, stepBox;
    juce::Slider   loopStartSlider, loopEndSlider, fadeSlider;
    juce::Label    loopLabel, loopStartLabel, loopEndLabel, rateLabel, stepLabel, fadeLabel;

    std::unique_ptr<SliderAttachment> volA, panA, pitchA, startA, endA, chokeA,
                                      loopStartA, loopEndA, fadeA;
    std::unique_ptr<ComboAttachment>  loopA, rateA, stepA;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackEditor)
};
