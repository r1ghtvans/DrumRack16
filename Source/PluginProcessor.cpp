#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
    }

    return layout;
}

DrumRackProcessor::DrumRackProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();

    for (int i = 0; i < numPads; ++i)
    {
        padNote[i].store (firstNote + i);
        hitCounter[i].store (0);
        guiTrigger[i].store (0.0f);

        const juce::String id = juce::String (i);
        volP[i]   = apvts.getRawParameterValue ("vol" + id);
        panP[i]   = apvts.getRawParameterValue ("pan" + id);
        pitchP[i] = apvts.getRawParameterValue ("pitch" + id);
        startP[i] = apvts.getRawParameterValue ("start" + id);
        endP[i]   = apvts.getRawParameterValue ("end" + id);
        chokeP[i] = apvts.getRawParameterValue ("choke" + id);
    }
}

void DrumRackProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& v : voices)
        v = Voice();
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
                startVoice (p, vel);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices)
            if (v.active)
                v.choking = true;
    }
    // Note Off игнорируем: это one-shot драм-машина
}

void DrumRackProcessor::previewPad (int pad, float velocity)
{
    if (juce::isPositiveAndBelow (pad, numPads))
        guiTrigger[pad].store (velocity);
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

        const double startPos = (double) juce::jlimit (0.0f, 1.0f, startP[p]->load()) * len;
        double endPos = (double) juce::jlimit (0.0f, 1.0f, endP[p]->load()) * len;
        endPos = juce::jlimit (startPos + 1.0, (double) (len - 1), endPos);

        const float* srcL = data->buffer.getReadPointer (0);
        const float* srcR = data->buffer.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            if (v.pos >= endPos)
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

            const double toEnd = endPos - v.pos;
            if (toEnd < 32.0)                       // короткий фейд на обрезанном конце
                g *= (float) (toEnd / 32.0);

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

    // клики мышью по пэдам из интерфейса
    for (int p = 0; p < numPads; ++p)
    {
        const float vel = guiTrigger[p].exchange (0.0f);
        if (vel > 0.0f)
            startVoice (p, vel);
    }

    // MIDI с точностью до сэмпла
    int pos = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (eventPos > pos)
        {
            renderRange (buffer, pos, eventPos);
            pos = eventPos;
        }

        handleMidi (metadata.getMessage());
    }

    renderRange (buffer, pos, numSamples);
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
