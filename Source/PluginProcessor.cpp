#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

juce::AudioProcessor::BusesProperties DrumRackProcessor::makeBuses()
{
    BusesProperties props;
    props = props.withOutput ("Out 1 (Main)", juce::AudioChannelSet::stereo(), true);

    for (int i = 1; i < DrumRackProcessor::numPads; ++i)
        props = props.withOutput ("Out " + juce::String (i + 1), juce::AudioChannelSet::stereo(), false);

    return props;
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumRackProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < numPads; ++i)
    {
        const String id = String (i);
        const String n  = "Pad " + String (i + 1) + " ";

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "vol" + id, 1 }, n + "Volume",
            NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "pan" + id, 1 }, n + "Pan",
            NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "pitch" + id, 1 }, n + "Pitch",
            NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("st")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "start" + id, 1 }, n + "Start",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "end" + id, 1 }, n + "End",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { "choke" + id, 1 }, n + "Choke Group", 0, 8, 0));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "loop" + id, 1 }, n + "Loop", loopChoices(), 0));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "loopstart" + id, 1 }, n + "Loop Start",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "loopend" + id, 1 }, n + "Loop End",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "rate" + id, 1 }, n + "Rate", rateChoices(), 2));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "step" + id, 1 }, n + "Ratchet Step", stepChoices(), 2));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "fade" + id, 1 }, n + "Ratchet Fade",
            NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    }

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "seqon", 1 }, "Sequencer On", true));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "swing", 1 }, "Swing",
        NormalisableRange<float> (50.0f, 75.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "pattern", 1 }, "Pattern", patternChoices(), 0));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "seqlen", 1 }, "Pattern Length", 1, 16, 16));

    return layout;
}

DrumRackProcessor::DrumRackProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();

    for (int pat = 0; pat < numPatterns; ++pat)
        for (int i = 0; i < numPads; ++i)
            stepMask[pat][i].store (0);

    seqOnP   = apvts.getRawParameterValue ("seqon");
    swingP   = apvts.getRawParameterValue ("swing");
    patternP = apvts.getRawParameterValue ("pattern");
    seqLenP  = apvts.getRawParameterValue ("seqlen");

    for (int i = 0; i < numPads; ++i)
    {
        padNote[i].store (firstNote + i);
        hitCounter[i].store (0);
        guiTrigger[i].store (0.0f);
        guiRelease[i].store (false);

        const juce::String id = juce::String (i);
        volP[i]   = apvts.getRawParameterValue ("vol" + id);
        panP[i]   = apvts.getRawParameterValue ("pan" + id);
        pitchP[i] = apvts.getRawParameterValue ("pitch" + id);
        startP[i] = apvts.getRawParameterValue ("start" + id);
        endP[i]   = apvts.getRawParameterValue ("end" + id);
        chokeP[i] = apvts.getRawParameterValue ("choke" + id);
        loopP[i]      = apvts.getRawParameterValue ("loop" + id);
        loopStartP[i] = apvts.getRawParameterValue ("loopstart" + id);
        loopEndP[i]   = apvts.getRawParameterValue ("loopend" + id);
        rateP[i]      = apvts.getRawParameterValue ("rate" + id);
        stepP[i]      = apvts.getRawParameterValue ("step" + id);
        fadeP[i]      = apvts.getRawParameterValue ("fade" + id);
    }
}

void DrumRackProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& v : voices)
        v = Voice();

    for (auto& p : pending)
        p = Pending();

    for (auto& h : hitCount)
        h = 0;

    for (auto& s : seqHeld)
        s = false;

    wasPlaying = false;
    currentStep.store (-1);
    sampleCounter = 0;
    currentAbs = 0;
}

bool DrumRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (const auto& bus : layouts.outputBuses)
        if (! bus.isDisabled() && bus != juce::AudioChannelSet::stereo())
            return false;

    return true;
}

// ------------------------------------------------------------------ MIDI / голоса

void DrumRackProcessor::startVoice (int p, float velocity)
{
    hitCounter[p].fetch_add (1);

    auto data = std::atomic_load (&samples[p]);
    if (data == nullptr)
        return;

    // choke-группа: гасим остальные пэды из той же группы (например, закрытый/открытый хэт)
    const int group = (int) std::lround (chokeP[p]->load());
    if (group > 0)
        for (int q = 0; q < numPads; ++q)
            if (q != p && voices[q].active && (int) std::lround (chokeP[q]->load()) == group)
                voices[q].choking = true;

    auto& v = voices[p];
    v.active = true;
    v.choking = false;
    v.chokeGain = 1.0f;
    v.velocity = velocity;
    v.pos = (double) juce::jlimit (0.0f, 1.0f, startP[p]->load()) * data->buffer.getNumSamples();
}

void DrumRackProcessor::releaseLoop (int p)
{
    if (voices[p].active && loopP[p]->load() >= 0.5f)
        voices[p].choking = true;
}

void DrumRackProcessor::addPending (int p, float velocity, juce::int64 due, int kind)
{
    for (auto& e : pending)
    {
        if (! e.active)
        {
            e.active = true;
            e.pad = p;
            e.velocity = velocity;
            e.due = due;
            e.kind = kind;
            return;
        }
    }
    // очередь заполнена: лишние повторы просто не играем
}

void DrumRackProcessor::firePending (juce::int64 now)
{
    for (auto& e : pending)
    {
        if (! e.active || e.due > now)
            continue;

        e.active = false;
        const int p = e.pad;

        if (e.kind == 0)
        {
            startVoice (p, e.velocity);
        }
        else if (e.kind == 1)
        {
            const bool loop = loopP[p]->load() >= 0.5f;

            // для Loop: если соседние шаги подряд включены, петля просто продолжает играть
            if (loop && seqHeld[p] && voices[p].active)
                continue;

            triggerHit (p, e.velocity);
            seqHeld[p] = loop;
        }
        else if (seqHeld[p])
        {
            releaseLoop (p);
            seqHeld[p] = false;
        }
    }
}

void DrumRackProcessor::releaseSeqHeld()
{
    for (int p = 0; p < numPads; ++p)
        if (seqHeld[p])
        {
            releaseLoop (p);
            seqHeld[p] = false;
        }
}

// Ставит в очередь все шаги, которые попадают в текущий блок.
// Позиция берётся из транспорта хоста (в долях четверти), свинг сдвигает каждый второй шаг.
void DrumRackProcessor::scheduleSteps (double ppqStart, int numSamples, double ppqPerSample, juce::int64 blockStart)
{
    const int pattern = juce::jlimit (0, numPatterns - 1, (int) std::lround (patternP->load()));
    const int len = juce::jlimit (1, 16, (int) std::lround (seqLenP->load()));
    const double swing = juce::jlimit (0.5, 0.75, (double) swingP->load() / 100.0);
    const double shift = (swing - 0.5) * 2.0;     // 0..0.5 длины шага
    const double stepLen = 0.25;                  // 1/16 = четверть четверти
    const double ppqEnd = ppqStart + (double) numSamples * ppqPerSample;

    const auto g0 = (juce::int64) std::floor (ppqStart / stepLen) - 1;
    const auto g1 = (juce::int64) std::floor (ppqEnd / stepLen) + 1;

    for (juce::int64 g = juce::jmax<juce::int64> (0, g0); g <= g1; ++g)
    {
        const double t = (double) g * stepLen + ((g & 1) != 0 ? shift * stepLen : 0.0);

        if (t < ppqStart || t >= ppqEnd)
            continue;

        const int step = (int) (g % len);
        const auto offset = (juce::int64) std::floor ((t - ppqStart) / ppqPerSample);
        const juce::int64 due = blockStart + juce::jlimit<juce::int64> (0, numSamples - 1, offset);

        for (int p = 0; p < numPads; ++p)
        {
            const bool on = ((stepMask[pattern][p].load() >> step) & 1) != 0;

            if (on)
                addPending (p, 0.85f, due, 1);
            else if (loopP[p]->load() >= 0.5f)
                addPending (p, 0.0f, due, 2);
        }
    }
}

bool DrumRackProcessor::getStep (int pattern, int pad, int step) const
{
    if (! juce::isPositiveAndBelow (pattern, numPatterns) || ! juce::isPositiveAndBelow (pad, numPads)
        || ! juce::isPositiveAndBelow (step, 16))
        return false;

    return ((stepMask[pattern][pad].load() >> step) & 1) != 0;
}

void DrumRackProcessor::setStep (int pattern, int pad, int step, bool on)
{
    if (! juce::isPositiveAndBelow (pattern, numPatterns) || ! juce::isPositiveAndBelow (pad, numPads)
        || ! juce::isPositiveAndBelow (step, 16))
        return;

    if (on)
        stepMask[pattern][pad].fetch_or (1 << step);
    else
        stepMask[pattern][pad].fetch_and (~(1 << step));
}

void DrumRackProcessor::clearPattern (int pattern)
{
    if (! juce::isPositiveAndBelow (pattern, numPatterns))
        return;

    for (int p = 0; p < numPads; ++p)
        stepMask[pattern][p].store (0);
}

juce::int64 DrumRackProcessor::nextPendingDue() const
{
    juce::int64 best = std::numeric_limits<juce::int64>::max();

    for (const auto& e : pending)
        if (e.active && e.due < best)
            best = e.due;

    return best;
}

// Rate: /4 и /2 пропускают удары, x2 и x4 делят удар на повторы (ratchet).
// В режиме Loop параметр Rate не используется.
void DrumRackProcessor::triggerHit (int p, float velocity)
{
    if (loopP[p]->load() >= 0.5f)
    {
        startVoice (p, velocity);
        return;
    }

    const int rate = juce::jlimit (0, 4, (int) std::lround (rateP[p]->load()));

    if (rate < 2)
    {
        const int div = (rate == 0) ? 4 : 2;
        const bool play = (hitCount[p] == 0);
        hitCount[p] = (hitCount[p] + 1) % div;

        if (play)
            startVoice (p, velocity);
        return;
    }

    const int n = (rate == 3) ? 2 : (rate == 4 ? 4 : 1);
    startVoice (p, velocity);

    if (n == 1)
        return;

    static const double stepBeats[4] = { 1.0, 0.5, 0.25, 0.125 };
    const int stepIndex = juce::jlimit (0, 3, (int) std::lround (stepP[p]->load()));
    const double stepSamples = stepBeats[stepIndex] * 60.0 / bpm * currentSampleRate;
    const double spacing = stepSamples / (double) n;
    const float fade = juce::jlimit (0.0f, 1.0f, fadeP[p]->load());

    for (int k = 1; k < n; ++k)
    {
        const auto offset = juce::jmax<juce::int64> (1, (juce::int64) std::llround (spacing * (double) k));
        const float vel = velocity * (1.0f - fade * (float) k / (float) n);
        addPending (p, vel, currentAbs + offset);
    }
}

void DrumRackProcessor::handleMidi (const juce::MidiMessage& m)
{
    // MIDI-канал специально игнорируем: пэды MPK Mini шлют на канале 10, клавиши на канале 1

    if (m.isNoteOn())
    {
        const int   note = m.getNoteNumber();
        const float vel  = m.getFloatVelocity();

        if (learning.load() && learnPad.load() >= 0)
        {
            const int target = learnPad.load();
            padNote[target].store (note);
            learning.store (false);
            startVoice (target, vel);
            return;
        }

        for (int p = 0; p < numPads; ++p)
            if (padNote[p].load() == note)
                triggerHit (p, vel);
    }
    else if (m.isNoteOff())
    {
        // Note Off нужен только режиму Loop; в one-shot он игнорируется
        const int note = m.getNoteNumber();

        for (int p = 0; p < numPads; ++p)
            if (padNote[p].load() == note)
                releaseLoop (p);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices)
            if (v.active)
                v.choking = true;

        for (auto& e : pending)
            e.active = false;
    }
}

void DrumRackProcessor::previewPad (int pad, float velocity)
{
    if (juce::isPositiveAndBelow (pad, numPads))
        guiTrigger[pad].store (velocity);
}

void DrumRackProcessor::previewRelease (int pad)
{
    if (juce::isPositiveAndBelow (pad, numPads))
        guiRelease[pad].store (true);
}

// ------------------------------------------------------------------ звук

void DrumRackProcessor::renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    const int n = endSample - startSample;
    if (n <= 0)
        return;

    const float chokeStep = 1.0f / (0.005f * (float) currentSampleRate);   // затухание 5 мс

    for (int p = 0; p < numPads; ++p)
    {
        auto& v = voices[p];
        if (! v.active)
            continue;

        auto data = std::atomic_load (&samples[p]);
        if (data == nullptr)
        {
            v.active = false;
            continue;
        }

        const int len = data->buffer.getNumSamples();
        if (len < 2)
        {
            v.active = false;
            continue;
        }

        // выход: Out 1 (Main) для пэда 1 и для пэдов, чей отдельный выход не включён
        int busIndex = 0;
        if (p > 0)
            if (auto* bus = getBus (false, p))
                if (bus->isEnabled())
                    busIndex = p;

        auto out = getBusBuffer (buffer, false, busIndex);
        if (out.getNumChannels() < 1)
            continue;

        float* outL = out.getWritePointer (0, startSample);
        float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : nullptr;

        // параметры
        const float volDb = volP[p]->load();
        const float gain  = volDb <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (volDb);
        const float pan   = juce::jlimit (-1.0f, 1.0f, panP[p]->load());
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gainL = std::cos (angle) * 1.41421356f;
        const float gainR = std::sin (angle) * 1.41421356f;

        const double ratio = (data->sampleRate / currentSampleRate)
                             * std::pow (2.0, (double) pitchP[p]->load() / 12.0);

        bool loop = loopP[p]->load() >= 0.5f;
        const double loopStart = (double) juce::jlimit (0.0f, 1.0f, loopStartP[p]->load()) * len;
        const double loopEnd = juce::jmin ((double) (len - 1),
                                           (double) juce::jlimit (0.0f, 1.0f, loopEndP[p]->load()) * len);
        if (loopEnd - loopStart < 16.0)
            loop = false;   // слишком короткая петля не имеет смысла

        const double startPos = (double) juce::jlimit (0.0f, 1.0f, startP[p]->load()) * len;
        double endPos = (double) juce::jlimit (0.0f, 1.0f, endP[p]->load()) * len;
        endPos = juce::jlimit (startPos + 1.0, (double) (len - 1), endPos);

        const float* srcL = data->buffer.getReadPointer (0);
        const float* srcR = data->buffer.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            if (loop)
            {
                if (v.pos >= loopEnd)
                    v.pos = loopStart + std::fmod (v.pos - loopStart, loopEnd - loopStart);
            }
            else if (v.pos >= endPos)
            {
                v.active = false;
                break;
            }

            const int   i0   = (int) v.pos;
            const int   i1   = juce::jmin (i0 + 1, len - 1);
            const float frac = (float) (v.pos - (double) i0);

            const float sL = srcL[i0] + (srcL[i1] - srcL[i0]) * frac;
            const float sR = srcR[i0] + (srcR[i1] - srcR[i0]) * frac;

            float g = gain * v.velocity * v.chokeGain;

            if (! loop)
            {
                const double toEnd = endPos - v.pos;
                if (toEnd < 32.0)                   // короткий фейд на обрезанном конце
                    g *= (float) (toEnd / 32.0);
            }

            if (v.choking)
            {
                v.chokeGain -= chokeStep;
                if (v.chokeGain <= 0.0f)
                {
                    v.active = false;
                    break;
                }
            }

            outL[i] += sL * g * gainL;
            if (outR != nullptr)
                outR[i] += sR * g * gainR;

            v.pos += ratio;
        }
    }
}

void DrumRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // темп и позиция транспорта хоста
    bool playing = false;
    bool havePpq = false;
    double ppq = 0.0;

    if (auto* ph = getPlayHead())
    {
        if (auto info = ph->getPosition())
        {
            if (auto b = info->getBpm())
                if (*b > 20.0)
                    bpm = *b;

            playing = info->getIsPlaying();

            if (auto q = info->getPpqPosition())
            {
                ppq = *q;
                havePpq = true;
            }
        }
    }

    const juce::int64 blockStart = sampleCounter;
    currentAbs = blockStart;

    // секвенсор работает только пока играет транспорт хоста
    if (playing && havePpq && seqOnP->load() >= 0.5f)
    {
        const double ppqPerSample = bpm / 60.0 / currentSampleRate;
        scheduleSteps (ppq, numSamples, ppqPerSample, blockStart);

        const int len = juce::jlimit (1, 16, (int) std::lround (seqLenP->load()));
        const auto g = (juce::int64) std::floor (juce::jmax (0.0, ppq) / 0.25);
        currentStep.store ((int) (g % len));
    }
    else
    {
        currentStep.store (-1);
    }

    if (wasPlaying && ! playing)
        releaseSeqHeld();

    wasPlaying = playing;

    // нажатия и отпускания мышью из интерфейса
    for (int p = 0; p < numPads; ++p)
    {
        const float vel = guiTrigger[p].exchange (0.0f);
        if (vel > 0.0f)
            triggerHit (p, vel);

        if (guiRelease[p].exchange (false))
            releaseLoop (p);
    }

    // MIDI и запланированные повторы обрабатываем с точностью до сэмпла
    auto it = midi.begin();
    const auto endIt = midi.end();
    int pos = 0;

    for (;;)
    {
        currentAbs = blockStart + pos;

        while (it != endIt && juce::jlimit (0, numSamples, (*it).samplePosition) <= pos)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }

        firePending (currentAbs);

        if (pos >= numSamples)
            break;

        int next = numSamples;

        if (it != endIt)
            next = juce::jmin (next, juce::jmax (pos + 1, juce::jlimit (0, numSamples, (*it).samplePosition)));

        const juce::int64 due = nextPendingDue();
        if (due != std::numeric_limits<juce::int64>::max())
        {
            const juce::int64 rel = juce::jmax<juce::int64> ((juce::int64) pos + 1, due - blockStart);
            next = (int) juce::jmin<juce::int64> ((juce::int64) next, rel);
        }

        renderRange (buffer, pos, next);
        pos = next;
    }

    // уровень на выходе для анимации интерфейса
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    outputLevel.store (juce::jmax (peak, outputLevel.load() * 0.9f));

    sampleCounter += numSamples;
    midi.clear();
}

// ------------------------------------------------------------------ сэмплы

bool DrumRackProcessor::loadSample (int pad, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (pad, numPads))
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0
        || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 60.0))   // не длиннее минуты
        return false;

    const int len = (int) reader->lengthInSamples;

    auto data = std::make_shared<SampleData>();
    data->sampleRate = reader->sampleRate;
    data->buffer.setSize (2, len);
    data->buffer.clear();
    reader->read (&data->buffer, 0, len, 0, true, true);

    if (reader->numChannels == 1)
        data->buffer.copyFrom (1, 0, data->buffer, 0, 0, len);

    std::atomic_store (&samples[pad], std::shared_ptr<const SampleData> (data));

    const juce::ScopedLock sl (pathLock);
    samplePaths[pad] = file.getFullPathName();
    return true;
}

void DrumRackProcessor::clearSample (int pad)
{
    if (! juce::isPositiveAndBelow (pad, numPads))
        return;

    std::atomic_store (&samples[pad], std::shared_ptr<const SampleData>());

    const juce::ScopedLock sl (pathLock);
    samplePaths[pad] = {};
}

juce::String DrumRackProcessor::getSampleName (int pad) const
{
    if (! juce::isPositiveAndBelow (pad, numPads))
        return {};

    const juce::ScopedLock sl (pathLock);
    return samplePaths[pad].isEmpty() ? juce::String()
                                      : juce::File (samplePaths[pad]).getFileNameWithoutExtension();
}

// ------------------------------------------------------------------ сохранение проекта

void DrumRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    {
        const juce::ScopedLock sl (pathLock);
        for (int i = 0; i < numPads; ++i)
        {
            state.setProperty ("path" + juce::String (i), samplePaths[i], nullptr);
            state.setProperty ("note" + juce::String (i), padNote[i].load(), nullptr);

            for (int pat = 0; pat < numPatterns; ++pat)
                state.setProperty ("steps" + juce::String (pat) + "_" + juce::String (i),
                                   stepMask[pat][i].load(), nullptr);
        }
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumRackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    for (int i = 0; i < numPads; ++i)
    {
        padNote[i].store ((int) tree.getProperty ("note" + juce::String (i), firstNote + i));

        for (int pat = 0; pat < numPatterns; ++pat)
            stepMask[pat][i].store ((int) tree.getProperty ("steps" + juce::String (pat) + "_" + juce::String (i), 0));

        const juce::String path = tree.getProperty ("path" + juce::String (i)).toString();
        const juce::File file (path);

        if (path.isNotEmpty() && file.existsAsFile())
            loadSample (i, file);
        else
            clearSample (i);
    }
}

juce::AudioProcessorEditor* DrumRackProcessor::createEditor()
{
    return new DrumRackEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumRackProcessor();
}
