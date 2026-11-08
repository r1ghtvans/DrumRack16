#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>

struct TestPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setBpm (bpm); i.setPpqPosition (ppq); i.setIsPlaying (playing); return i;
    }
};

static juce::File makeClick (float amp)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("click" + juce::String (amp) + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 2, 16, {}, 0));
    os.release();
    juce::AudioBuffer<float> b (2, 1000);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 1000; ++i) b.setSample (c, i, amp);
    w->writeFromAudioSampleBuffer (b, 0, 1000);
    return f;
}

static void setParam (DrumRackProcessor& p, const juce::String& id, float realValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

static std::vector<long> run (DrumRackProcessor& p, TestPlayHead& ph, int totalSamples, juce::MidiBuffer firstBlockMidi = {})
{
    std::vector<long> onsets;
    juce::AudioBuffer<float> buf (2, 512);
    long done = 0; bool prevZero = true; bool first = true;
    while (done < totalSamples)
    {
        ph.ppq = (double) done * ph.bpm / 60.0 / 44100.0;
        juce::MidiBuffer m; if (first) m = firstBlockMidi; first = false;
        p.processBlock (buf, m);
        for (int i = 0; i < 512; ++i)
        {
            const bool zero = std::abs (buf.getSample (0, i)) < 1e-6f;
            if (! zero && prevZero) onsets.push_back (done + i);
            prevZero = zero;
        }
        done += 512;
    }
    return onsets;
}

static void print (const char* name, const std::vector<long>& v)
{
    std::printf ("%s:", name); for (auto x : v) std::printf (" %ld", x); std::printf ("\n");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    DrumRackProcessor p;
    TestPlayHead ph;
    p.setPlayHead (&ph);
    p.prepareToPlay (44100.0, 512);

    p.loadSample (0, makeClick (0.5f));
    p.loadSample (1, makeClick (0.25f));

    // 1) kick на шагах 1 и 5 (ожидаем 0 и 22050)
    p.setStep (0, 0, 0, true); p.setStep (0, 0, 4, true);
    print ("test1 steps 0,4 (expect 0 22050)", run (p, ph, 30000));

    // 2) свинг 75% на нечётном шаге пэда 2 (ожидаем ~8269)
    p.clearPattern (0);
    p.setStep (0, 1, 1, true);
    setParam (p, "swing", 75.0f);
    print ("test2 swing75 step1 (expect ~8269)", run (p, ph, 12000));

    // 3) без свинга тот же шаг (ожидаем ~5512)
    setParam (p, "swing", 50.0f);
    print ("test3 no swing step1 (expect ~5512)", run (p, ph, 12000));

    // 4) транспорт стоит: ничего не играет
    ph.playing = false;
    print ("test4 stopped (expect none)", run (p, ph, 12000));
    ph.playing = true;

    // 5) длина паттерна 4: шаг 0 повторяется каждые 4 шага (ожидаем 0 22050 44100)
    p.clearPattern (0); p.setStep (0, 0, 0, true);
    setParam (p, "seqlen", 4.0f);
    print ("test5 len4 (expect 0 22050 44100)", run (p, ph, 50000));
    setParam (p, "seqlen", 16.0f);

    // 6) MIDI x2 ratchet: нота 36 на старте, второй удар через полшага (~2756)
    p.clearPattern (0);
    setParam (p, "seqon", 0.0f);
    setParam (p, "rate0", 3.0f);   // x2
    juce::MidiBuffer mb; mb.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
    print ("test6 ratchet x2 (expect 0 ~2756)", run (p, ph, 6000, mb));

    // 7) /2: каждый второй удар пропускается (считаем по счётчику ударов)
    setParam (p, "rate0", 1.0f);
    p.prepareToPlay (44100.0, 512);
    std::vector<long> all;
    for (int k = 0; k < 4; ++k)
    {
        const int before = p.hitCounter[0].load();
        juce::MidiBuffer m2; m2.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        run (p, ph, 512, m2);
        all.push_back (p.hitCounter[0].load() - before);
    }
    std::printf ("test7 /2 hits played (expect 1 0 1 0):"); for (auto x : all) std::printf (" %ld", x); std::printf ("\n");

    // 7b) /4
    setParam (p, "rate0", 0.0f);
    p.prepareToPlay (44100.0, 512);
    all.clear();
    for (int k = 0; k < 8; ++k)
    {
        const int before = p.hitCounter[0].load();
        juce::MidiBuffer m2; m2.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        run (p, ph, 512, m2);
        all.push_back (p.hitCounter[0].load() - before);
    }
    std::printf ("test7b /4 hits played (expect 1 0 0 0 1 0 0 0):"); for (auto x : all) std::printf (" %ld", x); std::printf ("\n");

    // 8) Loop: нота on, играет дольше сэмпла, note off затихает
    setParam (p, "rate0", 2.0f);
    setParam (p, "loop0", 1.0f);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
    p.processBlock (buf, on);
    float lastLevel = 0;
    for (int i = 0; i < 20; ++i) { juce::MidiBuffer e; p.processBlock (buf, e); lastLevel = std::abs (buf.getSample (0, 100)); }
    std::printf ("test8 loop still sounding after 20 blocks (expect >0): %.3f\n", lastLevel);
    juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
    p.processBlock (buf, off);
    juce::MidiBuffer e2; p.processBlock (buf, e2); p.processBlock (buf, e2);
    std::printf ("test8 after note-off (expect 0): %.3f\n", std::abs (buf.getSample (0, 100)));
    return 0;
}
