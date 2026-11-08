#include "PluginEditor.h"

//==============================================================================
// Пэд на странице PADS
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
        const auto rowCol = pal::rowColour (index);

        g.setColour (pal::cell.interpolatedWith (rowCol, flash));
        g.fillRoundedRectangle (r, 8.0f);

        // цветная полоска сверху: цвет пэда совпадает с цветом строки в секвенсоре
        g.setColour (rowCol.withAlpha (0.85f));
        g.fillRoundedRectangle (r.withHeight (5.0f).reduced (10.0f, 0.0f).translated (0.0f, 6.0f), 2.5f);

        g.setColour (isSelected ? pal::blue : pal::edge);
        g.drawRoundedRectangle (r, 8.0f, isSelected ? 2.5f : 1.0f);

        const auto textCol = flash > 0.5f ? pal::onPastel : pal::text;
        const int note = owner.proc.padNote[index].load();

        g.setColour (textCol);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (juce::String (index + 1), r.reduced (10.0f).withTrimmedTop (10.0f).removeFromTop (20.0f),
                    juce::Justification::topLeft);

        g.setColour (textCol.withAlpha (0.6f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 5),
                    r.reduced (10.0f).withTrimmedTop (10.0f).removeFromTop (20.0f), juce::Justification::topRight);

        auto name = owner.proc.getSampleName (index);
        g.setColour (name.isEmpty() ? pal::textDim : textCol);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (name.isEmpty() ? "drop sample" : name,
                          getLocalBounds().reduced (8, 0).withTrimmedTop (34),
                          juce::Justification::centred, 2);

        // метки режима: LOOP и множитель ритма
        const auto id = juce::String (index);
        const bool loop = owner.proc.apvts.getRawParameterValue ("loop" + id)->load() >= 0.5f;
        const int rate = (int) std::lround (owner.proc.apvts.getRawParameterValue ("rate" + id)->load());
        static const char* rateText[5] = { "/4", "/2", "", "x2", "x4" };

        const juce::String tag = loop ? juce::String ("LOOP") : juce::String (rateText[juce::jlimit (0, 4, rate)]);

        if (tag.isNotEmpty())
        {
            g.setColour (flash > 0.5f ? pal::onPastel : pal::khaki);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (tag, r.reduced (10.0f).removeFromBottom (16.0f), juce::Justification::bottomRight);
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
// Сетка шагов: 16 строк (пэды) x 16 шагов
class DrumRackEditor::StepGrid : public juce::Component
{
public:
    explicit StepGrid (DrumRackEditor& o) : owner (o), proc (o.proc) {}

    int playStep = -1;

    void paint (juce::Graphics& g) override
    {
        const int pattern = currentPattern();
        const int len = juce::jlimit (1, 16, (int) std::lround (proc.apvts.getRawParameterValue ("seqlen")->load()));

        // номера долей сверху
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        for (int beat = 0; beat < 4; ++beat)
        {
            const auto first = cellRect (0, beat * 4);
            g.setColour (pal::textDim);
            g.drawText (juce::String (beat + 1), (int) first.getX() + 4, 0, 30, rulerH, juce::Justification::centredLeft);
        }

        for (int row = 0; row < 16; ++row)
        {
            const auto rowCol = pal::rowColour (row);
            const auto name = proc.getSampleName (row);

            // подпись строки
            auto labelArea = juce::Rectangle<float> (0.0f, (float) rulerH + row * rowHeight(), (float) labelW - 8.0f, rowHeight());
            g.setColour (rowCol);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (juce::String (row + 1).paddedLeft ('0', 2), labelArea.withWidth (24.0f).toNearestInt(),
                        juce::Justification::centredLeft);
            g.setColour (name.isEmpty() ? pal::textDim : pal::text);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (name.isEmpty() ? "-" : name, labelArea.withTrimmedLeft (28.0f).toNearestInt(),
                        juce::Justification::centredLeft, true);

            for (int col = 0; col < 16; ++col)
            {
                const auto r = cellRect (row, col).reduced (1.5f);
                const bool on = proc.getStep (pattern, row, col);
                const bool inRange = col < len;

                juce::Colour fill = ((col / 4) % 2 == 0) ? pal::cell : pal::cellAlt;
                if (on)
                    fill = rowCol;

                g.setColour (inRange ? fill : fill.withAlpha (0.3f));
                g.fillRoundedRectangle (r, 4.0f);

                if (col == playStep)
                {
                    g.setColour (pal::sand.withAlpha (on ? 0.35f : 0.18f));
                    g.fillRoundedRectangle (r, 4.0f);
                    g.setColour (pal::sand);
                    g.drawRoundedRectangle (r, 4.0f, 1.5f);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.getPosition();
        int row = 0, col = 0;

        if (pt.x < labelW && pt.y >= rulerH)
        {
            labelPad = juce::jlimit (0, 15, (int) ((pt.y - rulerH) / rowHeight()));
            owner.selectPad (labelPad);
            proc.previewPad (labelPad, 0.8f);
            return;
        }

        if (cellAt (pt, row, col))
        {
            const int pattern = currentPattern();
            paintValue = ! proc.getStep (pattern, row, col);
            proc.setStep (pattern, row, col, paintValue);
            lastRow = row;
            lastCol = col;
            dragging = true;
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;

        int row = 0, col = 0;
        if (cellAt (e.getPosition(), row, col) && (row != lastRow || col != lastCol))
        {
            proc.setStep (currentPattern(), row, col, paintValue);
            lastRow = row;
            lastCol = col;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;

        if (labelPad >= 0)
        {
            proc.previewRelease (labelPad);
            labelPad = -1;
        }
    }

private:
    static constexpr int labelW = 150;
    static constexpr int rulerH = 20;
    static constexpr int gapW = 8;

    int currentPattern() const
    {
        return juce::jlimit (0, DrumRackProcessor::numPatterns - 1,
                             (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
    }

    float rowHeight() const { return (float) (getHeight() - rulerH) / 16.0f; }
    float cellWidth() const { return (float) (getWidth() - labelW - 3 * gapW) / 16.0f; }

    juce::Rectangle<float> cellRect (int row, int col) const
    {
        const float x = (float) labelW + (float) col * cellWidth() + (float) (col / 4) * (float) gapW;
        return { x, (float) rulerH + (float) row * rowHeight(), cellWidth(), rowHeight() };
    }

    bool cellAt (juce::Point<int> pt, int& row, int& col) const
    {
        if (pt.y < rulerH || pt.x < labelW)
            return false;

        row = juce::jlimit (0, 15, (int) ((float) (pt.y - rulerH) / rowHeight()));

        for (int c = 0; c < 16; ++c)
        {
            auto r = cellRect (row, c);
            r.setWidth (r.getWidth() + (c % 4 == 3 ? (float) gapW : 0.0f));   // щель после группы тоже попадает в ближайший шаг

            if (pt.x >= (int) r.getX() && pt.x < (int) r.getRight())
            {
                col = c;
                return true;
            }
        }
        return false;
    }

    DrumRackEditor& owner;
    DrumRackProcessor& proc;
    bool dragging = false;
    bool paintValue = true;
    int lastRow = -1, lastCol = -1;
    int labelPad = -1;
};

//==============================================================================
DrumRackEditor::DrumRackEditor (DrumRackProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    // --- вкладки ---
    addAndMakeVisible (padsTab);
    addAndMakeVisible (seqTab);
    padsTab.onClick = [this] { showPage (0); };
    seqTab.onClick  = [this] { showPage (1); };

    brand.setText ("DRUMRACK16", juce::dontSendNotification);
    brand.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    brand.setColour (juce::Label::textColourId, pal::khaki);
    brand.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (brand);

    // --- страница PADS ---
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        pads[i] = std::make_unique<PadComponent> (*this, i);
        addAndMakeVisible (*pads[i]);
        padsPage.push_back (pads[i].get());
        lastHit[i] = proc.hitCounter[i].load();
    }

    padTitle.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    padTitle.setColour (juce::Label::textColourId, pal::text);
    addAndMakeVisible (padTitle);

    noteInfo.setColour (juce::Label::textColourId, pal::textDim);
    addAndMakeVisible (noteInfo);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (learnButton);

    learnButton.setClickingTogglesState (true);

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

    for (juce::Component* c : { (juce::Component*) &padTitle, (juce::Component*) &noteInfo,
                                (juce::Component*) &loadButton, (juce::Component*) &clearButton,
                                (juce::Component*) &learnButton,
                                (juce::Component*) &volSlider, (juce::Component*) &panSlider,
                                (juce::Component*) &pitchSlider, (juce::Component*) &startSlider,
                                (juce::Component*) &endSlider, (juce::Component*) &chokeSlider,
                                (juce::Component*) &volLabel, (juce::Component*) &panLabel,
                                (juce::Component*) &pitchLabel, (juce::Component*) &startLabel,
                                (juce::Component*) &endLabel, (juce::Component*) &chokeLabel,
                                (juce::Component*) &loopBox, (juce::Component*) &rateBox,
                                (juce::Component*) &stepBox, (juce::Component*) &loopStartSlider,
                                (juce::Component*) &loopEndSlider, (juce::Component*) &fadeSlider,
                                (juce::Component*) &loopLabel, (juce::Component*) &loopStartLabel,
                                (juce::Component*) &loopEndLabel, (juce::Component*) &rateLabel,
                                (juce::Component*) &stepLabel, (juce::Component*) &fadeLabel })
        padsPage.push_back (c);

    // --- страница SEQUENCER ---
    grid = std::make_unique<StepGrid> (*this);
    addAndMakeVisible (*grid);

    addAndMakeVisible (seqOnButton);
    addAndMakeVisible (clearPatternButton);
    seqOnButton.setClickingTogglesState (true);
    clearPatternButton.onClick = [this]
    {
        proc.clearPattern ((int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
        grid->repaint();
    };

    setupCombo  (patternBox, patternLabel, "Pattern", DrumRackProcessor::patternChoices());
    setupSlider (swingSlider, swingLabel, "Swing", " %");
    setupSlider (lenSlider,   lenLabel,   "Pattern length", " steps");

    seqOnA   = std::make_unique<ButtonAttachment> (proc.apvts, "seqon", seqOnButton);
    patternA = std::make_unique<ComboAttachment>  (proc.apvts, "pattern", patternBox);
    swingA   = std::make_unique<SliderAttachment> (proc.apvts, "swing", swingSlider);
    lenA     = std::make_unique<SliderAttachment> (proc.apvts, "seqlen", lenSlider);

    for (juce::Component* c : { (juce::Component*) &seqOnButton, (juce::Component*) &clearPatternButton,
                                (juce::Component*) &patternBox, (juce::Component*) &swingSlider,
                                (juce::Component*) &lenSlider, (juce::Component*) &patternLabel,
                                (juce::Component*) &swingLabel, (juce::Component*) &lenLabel,
                                (juce::Component*) grid.get() })
        seqPage.push_back (c);

    selectPad (0);
    showPage (0);
    setSize (980, 540);
    startTimerHz (30);
}

DrumRackEditor::~DrumRackEditor()
{
    stopTimer();
    resetPadAttachments();
    seqOnA.reset(); patternA.reset(); swingA.reset(); lenA.reset();
    setLookAndFeel (nullptr);
}

void DrumRackEditor::resetPadAttachments()
{
    volA.reset(); panA.reset(); pitchA.reset(); startA.reset(); endA.reset(); chokeA.reset();
    loopStartA.reset(); loopEndA.reset(); fadeA.reset();
    loopA.reset(); rateA.reset(); stepA.reset();
}

void DrumRackEditor::setupSlider (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    s.setTextValueSuffix (suffix);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.0f));
    l.setColour (juce::Label::textColourId, pal::textDim);
    addAndMakeVisible (l);
}

void DrumRackEditor::setupCombo (juce::ComboBox& b, juce::Label& l, const juce::String& text, const juce::StringArray& items)
{
    b.addItemList (items, 1);
    addAndMakeVisible (b);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.0f));
    l.setColour (juce::Label::textColourId, pal::textDim);
    addAndMakeVisible (l);
}

void DrumRackEditor::showPage (int newPage)
{
    page = newPage;

    for (auto* c : padsPage)
        c->setVisible (page == 0);

    for (auto* c : seqPage)
        c->setVisible (page == 1);

    padsTab.setToggleState (page == 0, juce::dontSendNotification);
    seqTab.setToggleState (page == 1, juce::dontSendNotification);
    repaint();
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

    resetPadAttachments();

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

    // подсветка текущего шага
    const int step = proc.currentStep.load();
    if (page == 1)
    {
        grid->playStep = step;
        grid->repaint();
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
    g.fillAll (pal::bg);
}

void DrumRackEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // вкладки
    auto tabRow = area.removeFromTop (30);
    padsTab.setBounds (tabRow.removeFromLeft (110));
    tabRow.removeFromLeft (6);
    seqTab.setBounds (tabRow.removeFromLeft (130));
    brand.setBounds (tabRow);
    area.removeFromTop (12);

    // ---------------- страница PADS ----------------
    {
        auto pa = area;

        // сетка 4x4: пэд 1 внизу слева, как на MPK Mini
        auto padArea = pa.removeFromLeft (420);
        const int cellW = padArea.getWidth() / 4;
        const int cellH = padArea.getHeight() / 4;

        for (int i = 0; i < DrumRackProcessor::numPads; ++i)
        {
            const int col = i % 4;
            const int row = 3 - (i / 4);
            pads[i]->setBounds (padArea.getX() + col * cellW, padArea.getY() + row * cellH, cellW, cellH);
        }

        pa.removeFromLeft (16);

        padTitle.setBounds (pa.removeFromTop (30));
        noteInfo.setBounds (pa.removeFromTop (24));
        pa.removeFromTop (8);

        auto buttons = pa.removeFromTop (30);
        loadButton.setBounds  (buttons.removeFromLeft (130));
        buttons.removeFromLeft (8);
        clearButton.setBounds (buttons.removeFromLeft (70));
        buttons.removeFromLeft (8);
        learnButton.setBounds (buttons.removeFromLeft (110));

        pa.removeFromTop (14);

        auto colA = pa.removeFromLeft ((pa.getWidth() - 16) / 2);
        pa.removeFromLeft (16);
        auto colB = pa;

        auto place = [] (juce::Rectangle<int>& col, juce::Label& l, juce::Component& c)
        {
            auto row = col.removeFromTop (50);
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

    // ---------------- страница SEQUENCER ----------------
    {
        auto sa = area;
        auto head = sa.removeFromTop (46);

        auto labelled = [] (juce::Rectangle<int> r, juce::Label& l, juce::Component& c)
        {
            l.setBounds (r.removeFromTop (16));
            c.setBounds (r.removeFromTop (26));
        };

        seqOnButton.setBounds (head.removeFromLeft (100).withTrimmedTop (20));
        head.removeFromLeft (16);
        labelled (head.removeFromLeft (110), patternLabel, patternBox);
        head.removeFromLeft (16);
        labelled (head.removeFromLeft (240), swingLabel, swingSlider);
        head.removeFromLeft (16);
        labelled (head.removeFromLeft (240), lenLabel, lenSlider);
        clearPatternButton.setBounds (head.removeFromRight (120).withTrimmedTop (20));

        sa.removeFromTop (8);
        grid->setBounds (sa);
    }
}
