#include "PluginEditor.h"

//==============================================================================
// форматирование значений
static juce::String fmtDb (double v)    { return v <= -59.9 ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; }
static juce::String fmtPan (double v)
{
    const int p = (int) std::lround (std::abs (v) * 100.0);
    return p == 0 ? juce::String ("C") : (v < 0 ? juce::String ("L ") : juce::String ("R ")) + juce::String (p) + "%";
}
static juce::String fmtSt (double v)    { return (v > 0.004 ? juce::String ("+") : juce::String()) + juce::String (v, 1) + " st"; }
static juce::String fmtPct (double v)   { return juce::String ((int) std::lround (v * 100.0)) + "%"; }
static juce::String fmtChoke (double v) { const int g = (int) std::lround (v); return g == 0 ? juce::String ("off") : "G" + juce::String (g); }
static juce::String fmtSwing (double v) { return juce::String ((int) std::lround (v)) + "%"; }
static juce::String fmtLen (double v)   { return juce::String ((int) std::lround (v)); }

//==============================================================================
// Пэд на странице PADS
class DrumRackEditor::PadButton : public juce::Component,
                                  public juce::FileDragAndDropTarget
{
public:
    PadButton (DrumRackEditor& o, int i) : owner (o), index (i) {}

    float flash = 0.0f;
    bool  isSelected = false;

    void paint (juce::Graphics& g) override
    {
        const auto rowCol = pal::rowColour (index);
        auto r = getLocalBounds().withTrimmedRight (4).withTrimmedBottom (4).reduced (1);

        const auto base = pal::paper.interpolatedWith (rowCol, 0.45f);
        pixelBox (g, r, rowCol, base.interpolatedWith (rowCol, flash), 3, 3);

        if (isSelected)
        {
            g.setColour (pal::ink);
            g.strokePath (steppedRect (r, 3, 2), juce::PathStrokeType (2.0f));
        }

        const int note = owner.proc.padNote[index].load();

        g.setColour (pal::ink);
        g.setFont (pixelfont::title (9.0f));
        g.drawText (juce::String (index + 1).paddedLeft ('0', 2), r.reduced (9, 8).removeFromTop (14),
                    juce::Justification::topLeft, false);

        g.setColour (pal::inkSoft);
        g.setFont (pixelfont::vhs (20.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 5), r.reduced (9, 6).removeFromTop (18),
                    juce::Justification::topRight, false);

        const auto name = owner.proc.getSampleName (index);
        g.setColour (name.isEmpty() ? pal::inkSoft.withAlpha (0.6f) : pal::ink);
        g.setFont (pixelfont::vhs (22.0f));
        g.drawFittedText (name.isEmpty() ? "empty" : name, r.reduced (8, 0).withTrimmedTop (22).withTrimmedBottom (16),
                          juce::Justification::centred, 2);

        const auto id = juce::String (index);
        const bool loop = owner.proc.apvts.getRawParameterValue ("loop" + id)->load() >= 0.5f;
        const int rate = (int) std::lround (owner.proc.apvts.getRawParameterValue ("rate" + id)->load());
        static const char* rateText[5] = { "/4", "/2", "", "x2", "x4" };
        const juce::String tag = loop ? juce::String ("LOOP") : juce::String (rateText[juce::jlimit (0, 4, rate)]);

        if (tag.isNotEmpty())
        {
            g.setColour (pal::ink);
            g.setFont (pixelfont::vhs (20.0f));
            g.drawText (tag, r.reduced (9, 4).removeFromBottom (16), juce::Justification::bottomRight, false);
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
        const int rh = rowHeight();
        const int cw = cellWidth();

        // номера долей
        g.setFont (pixelfont::vhs (20.0f));
        g.setColour (pal::inkSoft);
        for (int beat = 0; beat < 4; ++beat)
            g.drawText (juce::String (beat + 1), cellX (beat * 4) + 2, 0, 30, rulerH, juce::Justification::centredLeft, false);

        for (int row = 0; row < 16; ++row)
        {
            const int y = rulerH + row * rh;
            const auto rowCol = pal::rowColour (row);
            const auto name = proc.getSampleName (row);

            if (row == owner.selected)
            {
                g.setColour (rowCol.withAlpha (0.45f));
                g.fillRect (0, y, labelW - 6, rh - 1);
            }

            g.setColour (rowCol);
            g.fillRect (3, y + rh / 2 - 3, 6, 6);

            g.setColour (pal::ink);
            g.setFont (pixelfont::title (8.0f));
            g.drawText (juce::String (row + 1).paddedLeft ('0', 2), 13, y, 20, rh, juce::Justification::centredLeft, false);

            g.setColour (name.isEmpty() ? pal::inkSoft.withAlpha (0.55f) : pal::inkSoft);
            g.setFont (pixelfont::vhs (19.0f));
            g.drawText (name.isEmpty() ? "-" : name, 38, y, labelW - 48, rh, juce::Justification::centredLeft, true);

            for (int col = 0; col < 16; ++col)
            {
                const juce::Rectangle<int> cell (cellX (col) + 1, y + 1, cw - 2, rh - 2);   // зазор 2 px между шагами
                const bool on = proc.getStep (pattern, row, col);
                const bool inRange = col < len;

                auto fill = (col / 4) % 2 == 0 ? pal::track : pal::track.darker (0.05f);
                if (on)
                    fill = rowCol;

                g.setColour (inRange ? fill : fill.withAlpha (0.35f));
                g.fillRect (cell);

                if (on && inRange)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.55f));
                    g.fillRect (cell.getX(), cell.getY(), cell.getWidth(), 2);
                }

                if (col == playStep)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.35f));
                    g.fillRect (cell);
                    g.setColour (pal::ink);
                    g.drawRect (cell, 2);
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
            labelPad = juce::jlimit (0, 15, (pt.y - rulerH) / rowHeight());
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
    static constexpr int labelW = 112;
    static constexpr int rulerH = 16;
    static constexpr int gapW = 6;

    int currentPattern() const
    {
        return juce::jlimit (0, DrumRackProcessor::numPatterns - 1,
                             (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
    }

    int rowHeight() const { return juce::jmax (8, (getHeight() - rulerH) / 16); }
    int cellWidth() const { return (getWidth() - labelW - 3 * gapW) / 16; }
    int cellX (int col) const { return labelW + col * cellWidth() + (col / 4) * gapW; }

    bool cellAt (juce::Point<int> pt, int& row, int& col) const
    {
        if (pt.y < rulerH || pt.x < labelW)
            return false;

        row = juce::jlimit (0, 15, (pt.y - rulerH) / rowHeight());

        for (int c = 15; c >= 0; --c)
            if (pt.x >= cellX (c))
            {
                col = c;
                return true;
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
    : AudioProcessorEditor (&p), proc (p),
      volRow ("Volume", fmtDb), panRow ("Pan", fmtPan), pitchRow ("Pitch", fmtSt),
      startRow ("Start", fmtPct), endRow ("End", fmtPct), chokeRow ("Choke", fmtChoke),
      loopChoice ("Loop", { "OFF", "LOOP" }, pal::lavender),
      rateChoice ("Rate", { "/4", "/2", "x1", "x2", "x4" }, pal::lavender),
      stepChoice ("Step", DrumRackProcessor::stepChoices(), pal::lavender),
      loopStartRow ("Loop in", fmtPct), loopEndRow ("Loop out", fmtPct), fadeRow ("Fade", fmtPct),
      patternChoice ("Pattern", DrumRackProcessor::patternChoices(), pal::sky),
      swingRow ("Swing", fmtSwing), lenRow ("Length", fmtLen)
{
    setLookAndFeel (&laf);
    setOpaque (true);
    setResizable (false, false);

    // --- вкладки ---
    addAndMakeVisible (padsTab);
    addAndMakeVisible (seqTab);
    padsTab.onClick = [this] { showPage (0); };
    seqTab.onClick  = [this] { showPage (1); };

    // --- страница PADS ---
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        pads[i] = std::make_unique<PadButton> (*this, i);
        addAndMakeVisible (*pads[i]);
        padsPage.push_back (pads[i].get());
        lastHit[i] = proc.hitCounter[i].load();
    }

    loadBtn.onClick  = [this] { browseForSample(); };
    clearBtn.onClick = [this] { proc.clearSample (selected); repaint(); };
    learnBtn.onClick = [this]
    {
        proc.learnPad.store (selected);
        proc.learning.store (learnBtn.isOn());
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &loadBtn, &clearBtn, &learnBtn,
             &volRow, &panRow, &pitchRow, &startRow, &endRow, &chokeRow,
             &loopChoice, &loopStartRow, &loopEndRow, &rateChoice, &stepChoice, &fadeRow })
    {
        addAndMakeVisible (c);
        padsPage.push_back (c);
    }

    // --- страница SEQUENCER ---
    grid = std::make_unique<StepGrid> (*this);

    patternChoice.attach (proc.apvts, "pattern");
    swingRow.attach (proc.apvts, "swing");
    lenRow.attach (proc.apvts, "seqlen");

    if (auto* seqParam = proc.apvts.getParameter ("seqon"))
    {
        seqOnAttachment = std::make_unique<juce::ParameterAttachment> (*seqParam, [this] (float v) { seqOnBtn.setOn (v >= 0.5f); });
        seqOnAttachment->sendInitialUpdate();
    }

    seqOnBtn.onClick = [this]
    {
        if (seqOnAttachment != nullptr)
            seqOnAttachment->setValueAsCompleteGesture (seqOnBtn.isOn() ? 1.0f : 0.0f);
    };

    clearPatBtn.onClick = [this]
    {
        proc.clearPattern ((int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
        grid->repaint();
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &patternChoice, &seqOnBtn, &clearPatBtn, &swingRow, &lenRow, grid.get() })
    {
        addAndMakeVisible (c);
        seqPage.push_back (c);
    }

    selectPad (0);
    showPage (0);
    setSize (windowW, windowH);
    startTimerHz (15);   // намеренно «рваный» ретро-темп
}

DrumRackEditor::~DrumRackEditor()
{
    stopTimer();
    seqOnAttachment.reset();
    setLookAndFeel (nullptr);
}

//==============================================================================
void DrumRackEditor::showPage (int newPage)
{
    page = newPage;

    for (auto* c : padsPage) c->setVisible (page == 0);
    for (auto* c : seqPage)  c->setVisible (page == 1);

    padsTab.setOn (page == 0);
    seqTab.setOn (page == 1);
    repaint();
}

void DrumRackEditor::selectPad (int index)
{
    selected = juce::jlimit (0, DrumRackProcessor::numPads - 1, index);

    if (proc.learning.load() && proc.learnPad.load() != selected)
    {
        proc.learning.store (false);
        learnBtn.setOn (false);
    }

    const auto id = juce::String (selected);
    volRow.attach       (proc.apvts, "vol" + id);
    panRow.attach       (proc.apvts, "pan" + id);
    pitchRow.attach     (proc.apvts, "pitch" + id);
    startRow.attach     (proc.apvts, "start" + id);
    endRow.attach       (proc.apvts, "end" + id);
    chokeRow.attach     (proc.apvts, "choke" + id);
    loopChoice.attach   (proc.apvts, "loop" + id);
    loopStartRow.attach (proc.apvts, "loopstart" + id);
    loopEndRow.attach   (proc.apvts, "loopend" + id);
    rateChoice.attach   (proc.apvts, "rate" + id);
    stepChoice.attach   (proc.apvts, "step" + id);
    fadeRow.attach      (proc.apvts, "fade" + id);

    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
        pads[i]->isSelected = (i == selected);

    repaint();
}

void DrumRackEditor::browseForSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");

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

//==============================================================================
// анимация (15 Гц)
void DrumRackEditor::timerCallback()
{
    advanceFrame();
    repaint();
}

void DrumRackEditor::advanceFrame()
{
    ++frame;

    // облака
    static const float speeds[3] = { 0.35f, 0.6f, 0.25f };
    for (int i = 0; i < 3; ++i)
    {
        cloudX[i] += speeds[i] * 2.0f;
        if (cloudX[i] > (float) windowW)
            cloudX[i] = -50.0f;
    }

    // подсветка пэдов
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        const int hits = proc.hitCounter[i].load();
        if (hits != lastHit[i])
        {
            lastHit[i] = hits;
            pads[i]->flash = 1.0f;
        }
        else
        {
            pads[i]->flash = pads[i]->flash > 0.08f ? pads[i]->flash * 0.55f : 0.0f;
        }
    }

    // подсветка текущего шага
    grid->playStep = proc.currentStep.load();

    // MIDI Learn
    const bool learning = proc.learning.load();
    if (learnBtn.isOn() != learning)
        learnBtn.setOn (learning);

    // эквалайзер
    const float level = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load());
    for (int i = 0; i < 48; ++i)
    {
        const float shape = 0.55f + 0.45f * std::sin ((float) frame * 0.7f + (float) i * 0.55f);
        const float target = juce::jlimit (0.0f, 1.0f, level * 1.7f * (0.35f * shape + 0.65f * rng.nextFloat()));
        eqHeights[i] = target > eqHeights[i] ? target : juce::jmax (target, eqHeights[i] - 0.14f);
    }

    // редкие «сбои»: примерно раз в 4 секунды на 2-4 кадра
    if (glitchFrames > 0)
    {
        --glitchFrames;
        glitchY = rng.nextInt (windowH - 60);
        glitchH = 10 + rng.nextInt (24);
    }
    else if (--framesToGlitch <= 0)
    {
        glitchFrames = 2 + rng.nextInt (3);
        framesToGlitch = 45 + rng.nextInt (30);
        glitchY = rng.nextInt (windowH - 60);
        glitchH = 10 + rng.nextInt (24);
    }
}

void DrumRackEditor::prepareSnapshot (int pageIndex, int frames, int glitch)
{
    showPage (pageIndex);

    for (int i = 0; i < frames; ++i)
        advanceFrame();

    glitchFrames = glitch;
    glitchY = 170;
    glitchH = 22;
}

//==============================================================================
void DrumRackEditor::rebuildBackground()
{
    background = juce::Image (juce::Image::RGB, windowW, windowH, false);
    juce::Graphics g (background);

    // горизонтальные полосы по 6 px, градиент квантован в 14 ступеней
    for (int y = 0; y < windowH; y += 6)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (float) (y + 3) / (float) windowH);
        const float q = juce::jmin (1.0f, std::floor (t * 14.0f) / 13.0f);

        const auto c = q < 0.5f ? pal::bgTop.interpolatedWith (pal::bgMid, q * 2.0f)
                                : pal::bgMid.interpolatedWith (pal::bgBot, (q - 0.5f) * 2.0f);
        g.setColour (c);
        g.fillRect (0, y, windowW, 6);
    }
}

void DrumRackEditor::rebuildScanlines()
{
    scanlines = juce::Image (juce::Image::ARGB, windowW, windowH, true);
    juce::Graphics g (scanlines);
    g.setColour (pal::ink.withAlpha (0.09f));

    for (int y = 0; y < windowH; y += 3)    // каждая 3-я строка пикселей
        g.fillRect (0, y, windowW, 1);
}

void DrumRackEditor::resized()
{
    rebuildBackground();
    rebuildScanlines();

    // вкладки в шапке
    padsTab.setBounds (24, 74, 100, 33);
    seqTab.setBounds (130, 74, 140, 33);

    const int top = 118, colW = 370, left = 20, right = 410;

    // --- PADS ---
    padsPanel   = { left, top, colW, 480 };
    samplePanel = { right, top, colW, 235 };
    loopPanel   = { right, top + 245, colW, 235 };

    const int gx = padsPanel.getX() + 12, gy = padsPanel.getY() + 46;
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        const int col = i % 4;
        const int row = 3 - (i / 4);   // пэд 1 внизу слева, как на MPK Mini
        pads[i]->setBounds (gx + col * 86, gy + row * 80, 86, 80);
    }

    const int by = padsPanel.getY() + 428;
    loadBtn.setBounds  (gx, by, 108, 33);
    clearBtn.setBounds (gx + 116, by, 108, 33);
    learnBtn.setBounds (gx + 232, by, 108, 33);

    auto placeRows = [] (juce::Rectangle<int> panel, std::initializer_list<juce::Component*> rows)
    {
        int y = panel.getY() + 44;
        for (auto* c : rows)
        {
            c->setBounds (panel.getX() + 12, y, panel.getWidth() - 24, 30);
            y += 30;
        }
    };

    placeRows (samplePanel, { &volRow, &panRow, &pitchRow, &startRow, &endRow, &chokeRow });
    placeRows (loopPanel,   { &loopChoice, &loopStartRow, &loopEndRow, &rateChoice, &stepChoice, &fadeRow });

    // --- SEQUENCER ---
    seqCtrlPanel = { left, top, 760, 120 };
    seqGridPanel = { left, top + 130, 760, 350 };

    const int cy = seqCtrlPanel.getY() + 44;
    patternChoice.setBounds (left + 12, cy, 352, 30);
    seqOnBtn.setBounds (left + 12, cy + 36, 120, 33);
    clearPatBtn.setBounds (left + 140, cy + 36, 120, 33);
    swingRow.setBounds (left + 12 + 352 + 16, cy, 344, 30);
    lenRow.setBounds (left + 12 + 352 + 16, cy + 32, 344, 30);

    grid->setBounds (seqGridPanel.getX() + 12, seqGridPanel.getY() + 42, 736, 296);
}

//==============================================================================
void DrumRackEditor::paintPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour c)
{
    pixelBox (g, r, c);

    // шапка панели: полоса цвета панели (прозрачность 0.45), сердечко и название
    g.saveState();
    g.reduceClipRegion (steppedRect (r.reduced (3), 3, 2));
    g.setColour (c.withAlpha (0.45f));
    g.fillRect (r.getX() + 3, r.getY() + 3, r.getWidth() - 6, 33);
    g.restoreState();

    drawSprite (g, sprites::miniHeart, sprites::miniHeartRows, r.getX() + 15, r.getY() + 15, 2,
                [] (char) { return pal::ink; });

    g.setColour (pal::ink);
    g.setFont (pixelfont::title (13.0f));
    g.drawText (title, r.getX() + 36, r.getY() + 3, r.getWidth() - 50, 33, juce::Justification::centredLeft, false);
}

void DrumRackEditor::paintHeader (juce::Graphics& g)
{
    // облака плывут по шапке
    const int cloudY[3] = { 10, 52, 22 };
    for (int i = 0; i < 3; ++i)
        drawSprite (g, sprites::cloud, sprites::cloudRows, (int) cloudX[i], cloudY[i], 3,
                    [] (char) { return juce::Colours::white.withAlpha (0.65f); });

    // блёстки мерцают
    const int sx[5] = { 330, 548, 430, 300, 770 };
    const int sy[5] = { 14, 56, 84, 70, 12 };
    for (int i = 0; i < 5; ++i)
    {
        const int phase = (frame + i * 3) % 8;
        if (phase < 6)
            drawSprite (g, phase < 3 ? sprites::sparkleA : sprites::sparkleB, sprites::sparkleRows, sx[i], sy[i], 3,
                        [i] (char) { return i % 2 == 0 ? pal::lemon : juce::Colours::white; });
    }

    // сердечки
    const int hx[3] = { 345, 505, 560 };
    const int hy[3] = { 46, 20, 90 };
    for (int i = 0; i < 3; ++i)
        drawSprite (g, sprites::heart, sprites::heartRows, hx[i], hy[i] + (((frame + i) / 2) % 2) * 2, 2,
                    [] (char) { return pal::pink; });

    // чиби-певицы: открывают рты и подпрыгивают, когда есть звук
    const bool singing = proc.outputLevel.load() > 0.02f;
    const juce::Colour hair[4]  = { pal::pink, pal::lavender, pal::mint, pal::sky };
    const juce::Colour dress[4] = { pal::sky, pal::lemon, pal::peach, pal::pink };

    for (int i = 0; i < 4; ++i)
    {
        const bool open = singing && ((frame + i) % 2 == 0);
        const int bounce = (singing && ((frame / 1 + i) % 2 == 0)) ? 4 : 0;
        const auto hc = hair[i].darker (0.12f);
        const auto dc = dress[i];

        drawSprite (g, open ? sprites::chibiOpen : sprites::chibiClosed, sprites::chibiRows,
                    582 + i * 52, 66 - bounce, 3,
                    [hc, dc] (char ch)
                    {
                        switch (ch)
                        {
                            case 'H': return hc;
                            case 'S': return juce::Colour (0xffFFE6D8);
                            case 'E': return pal::ink;
                            case 'B': return pal::pink;
                            case 'm': return pal::glitchPink;
                            case 'M': return pal::ink;
                            case 'D': return dc;
                            default:  return juce::Colours::white;
                        }
                    });
    }

    // заголовок с хроматической аберрацией (при сбое сдвиг ±4 px)
    const int shift = glitchFrames > 0 ? 4 : 2;
    const juce::Rectangle<int> t (24, 20, 400, 40);
    g.setFont (pixelfont::title (30.0f));

    g.setColour (pal::glitchCyan);
    g.drawText ("DRUMRACK16", t.translated (-shift, 0), juce::Justification::topLeft, false);
    g.setColour (pal::glitchPink);
    g.drawText ("DRUMRACK16", t.translated (shift, 0), juce::Justification::topLeft, false);
    g.setColour (pal::ink);
    g.drawText ("DRUMRACK16", t, juce::Justification::topLeft, false);
}

void DrumRackEditor::paintPadInfo (juce::Graphics& g)
{
    const auto name = proc.getSampleName (selected);
    const int note = proc.padNote[selected].load();
    const int x = padsPanel.getX() + 14;
    const int y = padsPanel.getY() + 46 + 320 + 4;

    g.setColour (pal::ink);
    g.setFont (pixelfont::vhs (26.0f));
    g.drawText ("Pad " + juce::String (selected + 1).paddedLeft ('0', 2) + "  " + (name.isEmpty() ? juce::String ("- empty -") : name),
                x, y, padsPanel.getWidth() - 28, 28, juce::Justification::centredLeft, true);

    g.setColour (pal::inkSoft);
    g.setFont (pixelfont::vhs (24.0f));
    g.drawText (proc.learning.load() ? juce::String ("hit a pad on your controller...")
                                     : "MIDI note " + juce::String (note) + "  (" + juce::MidiMessage::getMidiNoteName (note, true, true, 5) + ")",
                x, y + 26, padsPanel.getWidth() - 28, 26, juce::Justification::centredLeft, true);
}

void DrumRackEditor::paintEqualizer (juce::Graphics& g)
{
    const int x0 = 22, y0 = 612, segs = 8, segH = 4, segGap = 1, barW = 10, barPitch = 13;

    for (int i = 0; i < 48; ++i)
    {
        const int lit = (int) std::lround (eqHeights[i] * (float) segs);

        for (int s = 0; s < segs; ++s)
        {
            const int y = y0 + (segs - 1 - s) * (segH + segGap);
            if (s < lit)
                g.setColour (PixelLookAndFeel::segmentColour ((float) s / (float) (segs - 1)));
            else
                g.setColour (juce::Colours::white.withAlpha (0.35f));

            g.fillRect (x0 + i * barPitch, y, barW, segH);
        }
    }

    // подпись внизу справа, с тенью
    g.setFont (pixelfont::title (10.0f));
    const juce::Rectangle<int> sig (windowW - 20 - 120, windowH - 26, 120, 14);
    g.setColour (pal::ink.withAlpha (0.18f));
    g.drawText (juce::CharPointer_UTF8 ("\xD0\x9F\xD0\x95\xD0\x9F\xD0\x95L"), sig.translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (pal::inkSoft);
    g.drawText (juce::CharPointer_UTF8 ("\xD0\x9F\xD0\x95\xD0\x9F\xD0\x95L"), sig, juce::Justification::centredRight, false);
}

void DrumRackEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    paintHeader (g);

    if (page == 0)
    {
        paintPanel (g, padsPanel, "PADS", pal::pink);
        paintPanel (g, samplePanel, "SAMPLE", pal::mint);
        paintPanel (g, loopPanel, "LOOP / RATCHET", pal::lavender);
        paintPadInfo (g);
    }
    else
    {
        paintPanel (g, seqCtrlPanel, "SEQUENCER", pal::sky);
        paintPanel (g, seqGridPanel, "STEPS", pal::lavender);
    }

    paintEqualizer (g);
}

void DrumRackEditor::paintOverChildren (juce::Graphics& g)
{
    // сканлайны (кэшированный Image)
    g.drawImageAt (scanlines, 0, 0);

    // плёночное зерно: ~24 случайные точки 2x1 за кадр
    juce::Random grain (frame * 7919 + 13);
    for (int i = 0; i < 24; ++i)
    {
        g.setColour (grain.nextBool() ? juce::Colours::white.withAlpha (0.45f) : pal::ink.withAlpha (0.22f));
        g.fillRect (grain.nextInt (windowW), grain.nextInt (windowH), 2, 1);
    }

    // редкие сбои: полупрозрачная белая полоса с цветными разрывами
    if (glitchFrames > 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.24f));
        g.fillRect (0, glitchY, windowW, glitchH);

        juce::Random tear (frame * 104729 + glitchY);
        for (int k = 0; k < 4; ++k)
        {
            const int w = 40 + tear.nextInt (170);
            const int x = tear.nextInt (windowW - w);
            const int y = glitchY + tear.nextInt (juce::jmax (1, glitchH - 4));
            const int h = 2 + tear.nextInt (4);

            g.setColour (pal::glitchCyan.withAlpha (0.5f));
            g.fillRect (x - 4, y, w, h);
            g.setColour (pal::glitchPink.withAlpha (0.5f));
            g.fillRect (x + 4, y, w, h);
        }
    }
}
