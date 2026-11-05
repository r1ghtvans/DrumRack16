#include "PluginEditor.h"

static const juce::Colour accent (0xffff7a1a);
static const juce::Colour bgColour (0xff1b1c20);
static const juce::Colour padColour (0xff2d2f36);

//==============================================================================
class DrumRackEditor::PadComponent : public juce::Component,
                                     public juce::FileDragAndDropTarget
{
public:
    PadComponent (DrumRackEditor& o, int i) : owner (o), index (i) {}

    float flash = 0.0f;
    bool  isSelected = false;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);

        g.setColour (padColour.interpolatedWith (accent, flash));
        g.fillRoundedRectangle (r, 8.0f);

        g.setColour (isSelected ? accent : juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (r, 8.0f, isSelected ? 2.5f : 1.0f);

        const int note = owner.proc.padNote[index].load();

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (juce::String (index + 1), r.reduced (8.0f).removeFromTop (20.0f),
                    juce::Justification::topLeft);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 5),
                    r.reduced (8.0f).removeFromTop (20.0f), juce::Justification::topRight);

        auto name = owner.proc.getSampleName (index);
        g.setColour (name.isEmpty() ? juce::Colours::white.withAlpha (0.25f) : juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (name.isEmpty() ? "drop sample" : name,
                          getLocalBounds().reduced (8, 0).withTrimmedTop (26),
                          juce::Justification::centred, 2);

        // метки режима: LOOP и множитель ритма
        const auto id = juce::String (index);
        const bool loop = owner.proc.apvts.getRawParameterValue ("loop" + id)->load() >= 0.5f;
        const int rate = (int) std::lround (owner.proc.apvts.getRawParameterValue ("rate" + id)->load());
        static const char* rateText[5] = { "/4", "/2", "", "x2", "x4" };

        juce::String tag = loop ? juce::String ("LOOP") : juce::String (rateText[juce::jlimit (0, 4, rate)]);

        if (tag.isNotEmpty())
        {
            g.setColour (accent);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (tag, r.reduced (8.0f).removeFromBottom (16.0f), juce::Justification::bottomRight);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        owner.selectPad (index);
        owner.proc.previewPad (index, 0.8f);
    }

    void mouseUp (const juce::MouseEvent&) override { owner.proc.previewRelease (index); }

    void mouseDoubleClick (const juce::MouseEvent&) override { owner.browseForSample(); }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        for (auto& f : files)
            if (owner.proc.loadSample (index, juce::File (f)))
            {
                owner.selectPad (index);
                break;
            }
        repaint();
    }

private:
    DrumRackEditor& owner;
    int index;
};

//==============================================================================
DrumRackEditor::DrumRackEditor (DrumRackProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        pads[i] = std::make_unique<PadComponent> (*this, i);
        addAndMakeVisible (*pads[i]);
        lastHit[i] = proc.hitCounter[i].load();
    }

    padTitle.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    padTitle.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (padTitle);

    noteInfo.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (noteInfo);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (learnButton);

    learnButton.setClickingTogglesState (true);
    learnButton.setColour (juce::TextButton::buttonOnColourId, accent);

    loadButton.onClick  = [this] { browseForSample(); };
    clearButton.onClick = [this] { proc.clearSample (selected); repaint(); };
    learnButton.onClick = [this]
    {
        proc.learnPad.store (selected);
        proc.learning.store (learnButton.getToggleState());
    };

    setupSlider (volSlider,   volLabel,   "Volume",      " dB");
    setupSlider (panSlider,   panLabel,   "Pan",         "");
    setupSlider (pitchSlider, pitchLabel, "Pitch",       " st");
    setupSlider (startSlider, startLabel, "Start",       "");
    setupSlider (endSlider,   endLabel,   "End",         "");
    setupSlider (chokeSlider, chokeLabel, "Choke group (0 = off)", "");

    setupCombo  (loopBox, loopLabel, "Loop mode", DrumRackProcessor::loopChoices());
    setupSlider (loopStartSlider, loopStartLabel, "Loop start", "");
    setupSlider (loopEndSlider,   loopEndLabel,   "Loop end",   "");
    setupCombo  (rateBox, rateLabel, "MIDI rate", DrumRackProcessor::rateChoices());
    setupCombo  (stepBox, stepLabel, "Ratchet step (x2 / x4)", DrumRackProcessor::stepChoices());
    setupSlider (fadeSlider, fadeLabel, "Ratchet fade", "");

    selectPad (0);
    setSize (960, 480);
    startTimerHz (30);
}

DrumRackEditor::~DrumRackEditor()
{
    stopTimer();
    resetAttachments();
}

void DrumRackEditor::resetAttachments()
{
    volA.reset(); panA.reset(); pitchA.reset(); startA.reset(); endA.reset(); chokeA.reset();
    loopStartA.reset(); loopEndA.reset(); fadeA.reset();
    loopA.reset(); rateA.reset(); stepA.reset();
}

void DrumRackEditor::setupSlider (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::trackColourId, accent);
    s.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.0f));
    l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (l);
}

void DrumRackEditor::setupCombo (juce::ComboBox& b, juce::Label& l, const juce::String& text, const juce::StringArray& items)
{
    b.addItemList (items, 1);
    addAndMakeVisible (b);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.0f));
    l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (l);
}

void DrumRackEditor::selectPad (int index)
{
    selected = juce::jlimit (0, DrumRackProcessor::numPads - 1, index);

    // если шёл MIDI Learn для другого пэда, отменяем
    if (proc.learning.load() && proc.learnPad.load() != selected)
    {
        proc.learning.store (false);
        learnButton.setToggleState (false, juce::dontSendNotification);
    }

    resetAttachments();

    const auto id = juce::String (selected);
    volA   = std::make_unique<SliderAttachment> (proc.apvts, "vol"   + id, volSlider);
    panA   = std::make_unique<SliderAttachment> (proc.apvts, "pan"   + id, panSlider);
    pitchA = std::make_unique<SliderAttachment> (proc.apvts, "pitch" + id, pitchSlider);
    startA = std::make_unique<SliderAttachment> (proc.apvts, "start" + id, startSlider);
    endA   = std::make_unique<SliderAttachment> (proc.apvts, "end"   + id, endSlider);
    chokeA = std::make_unique<SliderAttachment> (proc.apvts, "choke" + id, chokeSlider);

    loopA      = std::make_unique<ComboAttachment>  (proc.apvts, "loop"      + id, loopBox);
    loopStartA = std::make_unique<SliderAttachment> (proc.apvts, "loopstart" + id, loopStartSlider);
    loopEndA   = std::make_unique<SliderAttachment> (proc.apvts, "loopend"   + id, loopEndSlider);
    rateA      = std::make_unique<ComboAttachment>  (proc.apvts, "rate"      + id, rateBox);
    stepA      = std::make_unique<ComboAttachment>  (proc.apvts, "step"      + id, stepBox);
    fadeA      = std::make_unique<SliderAttachment> (proc.apvts, "fade"      + id, fadeSlider);

    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
        pads[i]->isSelected = (i == selected);

    repaint();
}

void DrumRackEditor::browseForSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sample", juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg");

    juce::Component::SafePointer<DrumRackEditor> safeThis (this);
    const int pad = selected;

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis, pad] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (safeThis != nullptr && file.existsAsFile())
                              {
                                  safeThis->proc.loadSample (pad, file);
                                  safeThis->repaint();
                              }
                          });
}

void DrumRackEditor::timerCallback()
{
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        const int hits = proc.hitCounter[i].load();

        if (hits != lastHit[i])
        {
            lastHit[i] = hits;
            pads[i]->flash = 1.0f;
            pads[i]->repaint();
        }
        else if (pads[i]->flash > 0.01f)
        {
            pads[i]->flash *= 0.82f;
            pads[i]->repaint();
        }
        else if (pads[i]->flash > 0.0f)
        {
            pads[i]->flash = 0.0f;
            pads[i]->repaint();
        }
    }

    // MIDI Learn: когда нота получена, процессор сам выключает learning
    const bool learning = proc.learning.load();
    if (learnButton.getToggleState() != learning)
    {
        learnButton.setToggleState (learning, juce::dontSendNotification);
        pads[selected]->repaint();
    }

    const int note = proc.padNote[selected].load();
    auto name = proc.getSampleName (selected);

    padTitle.setText ("Pad " + juce::String (selected + 1) + (name.isEmpty() ? "" : "  -  " + name),
                      juce::dontSendNotification);

    noteInfo.setText (learning ? "Hit a pad on your MIDI controller..."
                               : "MIDI note " + juce::String (note) + "  ("
                                   + juce::MidiMessage::getMidiNoteName (note, true, true, 5) + ")",
                      juce::dontSendNotification);
}

void DrumRackEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
}

void DrumRackEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // сетка 4x4: пэд 1 внизу слева, как на MPK Mini
    auto padArea = area.removeFromLeft (420);
    const int cellW = padArea.getWidth() / 4;
    const int cellH = padArea.getHeight() / 4;

    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        const int col = i % 4;
        const int row = 3 - (i / 4);
        pads[i]->setBounds (padArea.getX() + col * cellW, padArea.getY() + row * cellH, cellW, cellH);
    }

    area.removeFromLeft (16);

    padTitle.setBounds (area.removeFromTop (30));
    noteInfo.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto buttons = area.removeFromTop (30);
    loadButton.setBounds  (buttons.removeFromLeft (130));
    buttons.removeFromLeft (8);
    clearButton.setBounds (buttons.removeFromLeft (70));
    buttons.removeFromLeft (8);
    learnButton.setBounds (buttons.removeFromLeft (110));

    area.removeFromTop (14);

    // две колонки
    auto colA = area.removeFromLeft ((area.getWidth() - 16) / 2);
    area.removeFromLeft (16);
    auto colB = area;

    auto place = [] (juce::Rectangle<int>& col, juce::Label& l, juce::Component& c)
    {
        auto row = col.removeFromTop (48);
        l.setBounds (row.removeFromTop (16));
        c.setBounds (row.removeFromTop (26));
    };

    place (colA, volLabel,   volSlider);
    place (colA, panLabel,   panSlider);
    place (colA, pitchLabel, pitchSlider);
    place (colA, startLabel, startSlider);
    place (colA, endLabel,   endSlider);
    place (colA, chokeLabel, chokeSlider);

    place (colB, loopLabel,      loopBox);
    place (colB, loopStartLabel, loopStartSlider);
    place (colB, loopEndLabel,   loopEndSlider);
    place (colB, rateLabel,      rateBox);
    place (colB, stepLabel,      stepBox);
    place (colB, fadeLabel,      fadeSlider);
}
