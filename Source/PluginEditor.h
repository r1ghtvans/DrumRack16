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

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void selectPad (int index);
    void browseForSample();
    void timerCallback() override;
    void setupSlider (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix);

    DrumRackProcessor& proc;
    int selected = 0;

    std::unique_ptr<PadComponent> pads[DrumRackProcessor::numPads];
    int lastHit[DrumRackProcessor::numPads] {};

    juce::Label padTitle, noteInfo;
    juce::TextButton loadButton { "Load sample..." }, clearButton { "Clear" }, learnButton { "MIDI Learn" };

    juce::Slider volSlider, panSlider, pitchSlider, startSlider, endSlider, chokeSlider;
    juce::Label  volLabel, panLabel, pitchLabel, startLabel, endLabel, chokeLabel;

    std::unique_ptr<Attachment> volA, panA, pitchA, startA, endA, chokeA;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackEditor)
};
