#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>

// Загруженный сэмпл (всегда 2 канала)
struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

class DrumRackProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numPads = 16;
    static constexpr int firstNote = 36;   // пэд 1 = нота 36 (в FL называется C3), пэд 16 = нота 51

    DrumRackProcessor();
    ~DrumRackProcessor() override = default;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DrumRack16"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- вызывается из интерфейса (message thread) ---
    bool loadSample (int pad, const juce::File& file);
    void clearSample (int pad);
    juce::String getSampleName (int pad) const;
    void previewPad (int pad, float velocity);   // нажатие мышью на пэд
    void previewRelease (int pad);               // отпускание мыши (для режима Loop)

    static juce::StringArray loopChoices() { return { "Off", "Loop" }; }
    static juce::StringArray rateChoices() { return { "/4 (every 4th hit)", "/2 (every 2nd hit)", "x1 (normal)", "x2 (double)", "x4 (quadruple)" }; }
    static juce::StringArray stepChoices() { return { "1/4", "1/8", "1/16", "1/32" }; }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<int>   padNote[numPads];     // MIDI-нота каждого пэда
    std::atomic<int>   hitCounter[numPads];  // растёт при каждом ударе (для подсветки)
    std::atomic<bool>  learning { false };   // режим MIDI Learn
    std::atomic<int>   learnPad { -1 };      // какому пэду назначаем ноту

private:
    struct Voice
    {
        bool   active = false;
        double pos = 0.0;
        float  velocity = 1.0f;
        bool   choking = false;
        float  chokeGain = 1.0f;
    };

    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Pending
    {
        bool active = false;
        int pad = 0;
        float velocity = 1.0f;
        juce::int64 due = 0;
    };

    void handleMidi (const juce::MidiMessage& m);
    void triggerHit (int pad, float velocity);      // учитывает Loop и Rate
    void startVoice (int pad, float velocity);      // запускает звук сразу
    void releaseLoop (int pad);
    void addPending (int pad, float velocity, juce::int64 due);
    void firePending (juce::int64 now);
    juce::int64 nextPendingDue() const;
    void renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample);

    Voice  voices[numPads];
    double currentSampleRate = 44100.0;

    std::shared_ptr<const SampleData> samples[numPads];   // доступ через std::atomic_load/store
    juce::String samplePaths[numPads];
    mutable juce::CriticalSection pathLock;

    std::atomic<float>* volP[numPads]   {};
    std::atomic<float>* panP[numPads]   {};
    std::atomic<float>* pitchP[numPads] {};
    std::atomic<float>* startP[numPads] {};
    std::atomic<float>* endP[numPads]   {};
    std::atomic<float>* chokeP[numPads] {};
    std::atomic<float>* loopP[numPads]      {};
    std::atomic<float>* loopStartP[numPads] {};
    std::atomic<float>* loopEndP[numPads]   {};
    std::atomic<float>* rateP[numPads]      {};
    std::atomic<float>* stepP[numPads]      {};
    std::atomic<float>* fadeP[numPads]      {};

    static constexpr int maxPending = 128;
    Pending pending[maxPending];
    int hitCount[numPads] {};
    juce::int64 sampleCounter = 0;   // сколько сэмплов прошло с начала работы
    juce::int64 currentAbs = 0;      // текущая позиция внутри блока в тех же единицах
    double bpm = 120.0;

    std::atomic<float> guiTrigger[numPads];
    std::atomic<bool>  guiRelease[numPads];

    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackProcessor)
};
