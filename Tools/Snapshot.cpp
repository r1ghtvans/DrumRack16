// Снимок интерфейса в PNG без DAW:  DrumRack16_Snapshot <папка для PNG>
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static juce::File makeSample (const juce::String& name, float amp)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 2, 16, {}, 0));
    os.release();
    juce::AudioBuffer<float> b (2, 4000);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 4000; ++i)
            b.setSample (c, i, amp * std::sin ((float) i * 0.05f) * (1.0f - (float) i / 4000.0f));
    w->writeFromAudioSampleBuffer (b, 0, 4000);
    return f;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : ".");
    outDir.createDirectory();

    DrumRackProcessor proc;
    proc.prepareToPlay (44100.0, 512);

    const char* names[] = { "kick", "snare", "clap", "closed hat", "open hat", "rim", "shaker", "tom low", "tom hi", "crash" };
    for (int i = 0; i < 10; ++i)
        proc.loadSample (i, makeSample (names[i], 0.5f));

    // демо-паттерн
    for (int s : { 0, 4, 10, 12 }) proc.setStep (0, 0, s, true);
    for (int s : { 4, 12 })        proc.setStep (0, 1, s, true);
    for (int s : { 4, 12 })        proc.setStep (0, 2, s, true);
    for (int s = 0; s < 16; s += 2) proc.setStep (0, 3, s, true);
    for (int s : { 6, 14 })        proc.setStep (0, 4, s, true);
    for (int s : { 7, 9, 15 })     proc.setStep (0, 5, s, true);
    for (int s = 0; s < 16; ++s)   proc.setStep (0, 6, s, true);

    proc.currentStep.store (5);
    proc.outputLevel.store (0.7f);

    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* ed = dynamic_cast<DrumRackEditor*> (base.get());

    ed->prepareSnapshot (0, 9, 0);
    proc.hitCounter[2].fetch_add (1);
    outDir.getChildFile ("pads.png").deleteFile();
    {
        juce::FileOutputStream os (outDir.getChildFile ("pads.png"));
        juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), os);
    }

    ed->prepareSnapshot (1, 4, 3);
    outDir.getChildFile ("sequencer.png").deleteFile();
    {
        juce::FileOutputStream os (outDir.getChildFile ("sequencer.png"));
        juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), os);
    }
    return 0;
}
