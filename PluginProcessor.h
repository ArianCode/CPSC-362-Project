#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

class MyPluginAudioProcessorEditor;

class MyPluginAudioProcessor : public juce::AudioProcessor
{
public:
    MyPluginAudioProcessor();
    ~MyPluginAudioProcessor() override = default;

    // === JUCE overrides ===
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MyPlugin"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter layout factory (used in ctor)
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Expose parameters so the Editor can attach sliders
    juce::AudioProcessorValueTreeState valueTreeState;
    
    // Waveform data for visualization
    struct WaveformData
    {
        std::array<float, 512> buffer{};
        std::atomic<int> writeIndex{0};
        std::atomic<bool> hasNewData{false};
    };
    WaveformData waveformData;

private:
    // ===== Delay & Granular State =====
    static constexpr int maxDelayTime = 192000; // ~4s @ 48kHz
    std::array<std::vector<float>, 2> delayBuffers; // [channel][sample]
    std::array<int, 2> delayWriteIndex { 0, 0 };

    struct Grain
    {
        int   startPos   = 0;
        int   size       = 0;
        int   position   = 0;
        bool  isActive   = false;
        bool  isReverse  = false;
        float amplitude  = 1.0f;
    };

    static constexpr int maxGrains = 32;
    std::array<std::array<Grain, maxGrains>, 2> activeGrains {};
    std::array<int, 2> grainTriggerCountdown { 0, 0 };

    // Simple one-pole filter state per channel
    std::array<float, 2> highCutState { 0.0f, 0.0f };
    std::array<float, 2> lowCutState  { 0.0f, 0.0f };

    // ===== Advanced DSP Components =====
    
    // Filter
    juce::dsp::StateVariableTPTFilter<float> stateVariableFilter;
    
    // Pitch Shifting (using simple granular approach)
    std::array<std::vector<float>, 2> pitchBuffer;
    std::array<int, 2> pitchWriteIndex{0, 0};
    std::array<juce::SmoothedValue<float>, 2> pitchSmoother;

    juce::SmoothedValue<float> flangerFeedbackSmoother;
    
    // Chorus & Flanger
    juce::dsp::Chorus<float> chorus;

    // stereo flanger delay lines, one per channel
    juce::dsp::Chorus<float> flanger;

    // LFO
    struct LFOState
    {
        float phase = 0.0f;
        float frequency = 1.0f;
        int target = 0; // 0=cutoff, 1=pan
        bool isBipolar = true;
        int waveform = 0; // 0=sine, 1=triangle
        bool isTempoSync = false;
        int syncDivision = 2; // 0=1/4, 1=1/2, 2=1bar
    };
    LFOState lfoState;
    
    // Pan
    std::array<juce::SmoothedValue<float>, 2> panSmoother;
    
    // Waveform capture
    int waveformDownsampleCounter = 0;
    static constexpr int waveformDownsampleRate = 64; // Capture every 64th sample

    juce::Random random;
    double currentSampleRate = 44100.0;
    int currentBufferSize = 512;

    // Helpers used by processBlock
    void  triggerNewGrain (int channel, int grainSize, float spray, bool reverse, float randomization);
    float processActiveGrains (int channel, float* delayBuffer);
    float applyEQFiltering   (float sample, int channel, float highCut, float lowCut);
    
    // New advanced processing helpers
    void processFilter(juce::AudioBuffer<float>& buffer);
    void processPitchShift(juce::AudioBuffer<float>& buffer);
    void processChorus(juce::AudioBuffer<float>& buffer);
    void processFlanger(juce::AudioBuffer<float>& buffer);
    void processPanning(juce::AudioBuffer<float>& buffer);
    void updateLFO();
    float getLFOValue();
    void captureWaveformData(const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyPluginAudioProcessor)
};