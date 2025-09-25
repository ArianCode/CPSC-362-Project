#include <cmath>
#include "PluginProcessor.h"
#include "PluginEditor.h"

MyPluginAudioProcessor::MyPluginAudioProcessor()
: AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  valueTreeState (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Initialize pitch smoothers
    for (auto& smoother : pitchSmoother)
        smoother.reset(44100.0);
        
    // Initialize flanger feedback smoothers  
    flangerFeedbackSmoother.setCurrentAndTargetValue(0.0f);
        
    // Initialize pan smoothers
    for (auto& smoother : panSmoother)
        smoother.reset(44100.0);
}

juce::AudioProcessorEditor* MyPluginAudioProcessor::createEditor()
{
    return new MyPluginAudioProcessorEditor(*this);
}

void MyPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBufferSize = samplesPerBlock;
    
    // Prepare existing delay buffers
    for (auto ch = 0; ch < getTotalNumOutputChannels(); ++ch)
    {
        delayBuffers[ch].assign(maxDelayTime, 0.0f);
        delayWriteIndex[ch] = 0;
        highCutState[ch] = 0.0f;
        lowCutState[ch] = 0.0f;
        grainTriggerCountdown[ch] = 0;

        for (auto& g : activeGrains[ch]) g = Grain{};
    }
    
    // Prepare DSP chain
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    
    // State Variable Filter
    stateVariableFilter.prepare(spec);
    stateVariableFilter.reset();
    
    // Chorus
    chorus.prepare(spec);
    chorus.reset();
    
    // Flanger delay
    flanger.prepare(spec);
    flanger.reset();
    flangerFeedbackSmoother.reset(sampleRate, 0.02f);
    flangerFeedbackSmoother.setCurrentAndTargetValue(0.0f);
    // Initialize pitch buffers
    for (auto ch = 0; ch < getTotalNumOutputChannels(); ++ch)
    {
        pitchBuffer[ch].assign(8192, 0.0f); // Buffer for pitch shifting
        pitchWriteIndex[ch] = 0;
    }
    
    // Initialize smoothed values
    for (auto& smoother : pitchSmoother)
    {
        smoother.reset(sampleRate, 0.05); // 50ms smoothing
        smoother.setCurrentAndTargetValue(1.0f);
    }
    
    for (auto& smoother : panSmoother)
    {
        smoother.reset(sampleRate, 0.05);
        smoother.setCurrentAndTargetValue(0.0f); // Center pan
    }
    
    // Reset LFO
    lfoState.phase = 0.0f;
}

void MyPluginAudioProcessor::releaseResources()
{
    // Clean up if needed
}

bool MyPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void MyPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear extra channels
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // === ORIGINAL GRANULAR DELAY PROCESSING ===
    
    // Read parameter values  
    const float delayTime = *valueTreeState.getRawParameterValue("delayTime");
    const float feedback = *valueTreeState.getRawParameterValue("feedback");
    const float mix = *valueTreeState.getRawParameterValue("mix") / 100.0f;
    const float grainSize = *valueTreeState.getRawParameterValue("grainSize");
    const float grainDensity = *valueTreeState.getRawParameterValue("grainDensity");
    const float grainSpray = *valueTreeState.getRawParameterValue("grainSpray") / 100.0f;
    const float stereoWidth = *valueTreeState.getRawParameterValue("stereoWidth") / 100.0f;
    const float eqHigh = *valueTreeState.getRawParameterValue("eqHigh");
    const float eqLow = *valueTreeState.getRawParameterValue("eqLow");
    const bool reverseGrains = *valueTreeState.getRawParameterValue("reverseGrains") > 0.5f;
    const float randomization = *valueTreeState.getRawParameterValue("randomization") / 100.0f;

    // Create dry buffer for mixing
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Process original granular delay
    const int delaySamples = juce::jlimit(1, maxDelayTime - 1,
        static_cast<int>(delayTime * getSampleRate() / 1000.0f));

    int grainSizeSamples = static_cast<int>(grainSize * getSampleRate() / 1000.0f);
    grainSizeSamples = juce::jlimit(64, 8192, grainSizeSamples);

    // Process channels for granular delay
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* delayBuffer = delayBuffers[channel].data();

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float inputSample = channelData[sample];
            float delayedSample = 0.0f;

            // === GRANULAR PROCESSING ===
            if (grainDensity > 0.1f)
            {
                if (--grainTriggerCountdown[channel] <= 0)
                {
                    triggerNewGrain(channel, grainSizeSamples, grainSpray, reverseGrains, randomization);

                    const float densityFactor = juce::jmap(grainDensity, 0.1f, 4.0f, 0.1f, 4.0f);
                    const int baseInterval = static_cast<int>(grainSizeSamples * 0.5f / densityFactor);
                    const int randomVariation = static_cast<int>(baseInterval * randomization * (random.nextFloat() * 2.0f - 1.0f));
                    grainTriggerCountdown[channel] = juce::jmax(1, baseInterval + randomVariation);
                }

                delayedSample = processActiveGrains(channel, delayBuffer);
            }
            else
            {
                delayedSample = delayBuffer[delayWriteIndex[channel]];
            }

            // === EQ FILTERING ===
            delayedSample = applyEQFiltering(delayedSample, channel, eqHigh, eqLow);

            // === FEEDBACK PROCESSING ===
            float feedbackSample = inputSample + (delayedSample * feedback);
            feedbackSample = juce::jlimit(-1.0f, 1.0f, feedbackSample);
            if (std::abs(feedbackSample) > 0.95f)
            {
                feedbackSample = feedbackSample > 0 ?
                    0.95f + 0.05f * std::tanh((feedbackSample - 0.95f) * 10.0f) :
                   -0.95f + 0.05f * std::tanh((feedbackSample + 0.95f) * 10.0f);
            }

            delayBuffer[delayWriteIndex[channel]] = feedbackSample;

            // === STEREO PROCESSING ===
            if (buffer.getNumChannels() == 2 && stereoWidth > 0.0f)
            {
                if (channel == 0)
                {
                    int crossDelay = static_cast<int>(delaySamples * 0.7f);
                    crossDelay = (delayWriteIndex[channel] - crossDelay + maxDelayTime) % maxDelayTime;
                    delayedSample += delayBuffers[1][crossDelay] * stereoWidth * 0.3f;
                }
                else
                {
                    int crossDelay = static_cast<int>(delaySamples * 0.8f);
                    crossDelay = (delayWriteIndex[channel] - crossDelay + maxDelayTime) % maxDelayTime;
                    delayedSample += delayBuffers[0][crossDelay] * stereoWidth * 0.3f;
                }
            }

            // Mix dry and delay
            channelData[sample] = inputSample * (1.0f - mix) + delayedSample * mix;
            delayWriteIndex[channel] = (delayWriteIndex[channel] + 1) % maxDelayTime;
        }
    }

    // === NEW ADVANCED PROCESSING ===
    updateLFO();
    
    processFilter(buffer);
    processPitchShift(buffer);
    processChorus(buffer);
    processFlanger(buffer);
    processPanning(buffer);
    
    // Capture waveform data
    captureWaveformData(buffer);
}

// === Advanced Processing Methods ===

void MyPluginAudioProcessor::updateLFO()
{
    const float lfoRate = *valueTreeState.getRawParameterValue("lfoRate");
    const bool tempoSync = *valueTreeState.getRawParameterValue("lfoTempoSync") > 0.5f;
    
    if (tempoSync)
    {
        auto playHead = getPlayHead();
        if (playHead != nullptr)
        {
            auto position = playHead->getPosition();
            if (position.hasValue())
            {
                auto bpm = position->getBpm();
                if (bpm.hasValue())
                {
                    const float syncDivision = *valueTreeState.getRawParameterValue("lfoSyncDivision");
                    float beatsPerSecond = *bpm / 60.0f;
                    float divisor = syncDivision == 0 ? 4.0f : (syncDivision == 1 ? 2.0f : 1.0f);
                    lfoState.frequency = beatsPerSecond / divisor;
                }
            }
        }
    }
    else
    {
        lfoState.frequency = juce::jmap(lfoRate, 0.0f, 100.0f, 0.1f, 10.0f);
    }
    
    // Update phase
    lfoState.phase += lfoState.frequency * (1.0f / currentSampleRate) * currentBufferSize;
    while (lfoState.phase >= 1.0f)
        lfoState.phase -= 1.0f;
}

float MyPluginAudioProcessor::getLFOValue()
{
    const float waveform = *valueTreeState.getRawParameterValue("lfoWaveform");
    const bool bipolar = *valueTreeState.getRawParameterValue("lfoBipolar") > 0.5f;
    
    float value = 0.0f;
    
    if (waveform < 0.5f) // Sine
    {
        value = std::sin(lfoState.phase * 2.0f * juce::MathConstants<float>::pi);
    }
    else // Triangle
    {
        float trianglePhase = lfoState.phase * 4.0f;
        if (trianglePhase < 1.0f)
            value = trianglePhase;
        else if (trianglePhase < 3.0f)
            value = 2.0f - trianglePhase;
        else
            value = trianglePhase - 4.0f;
    }
    
    if (!bipolar)
        value = (value + 1.0f) * 0.5f; // Convert to 0-1 range
        
    return value;
}

void MyPluginAudioProcessor::processFilter(juce::AudioBuffer<float>& buffer)
{
    const float cutoff = *valueTreeState.getRawParameterValue("filterCutoff");
    const float resonance = *valueTreeState.getRawParameterValue("filterResonance");
    const float filterType = *valueTreeState.getRawParameterValue("filterType");
    const float lfoDepth = *valueTreeState.getRawParameterValue("lfoDepth");
    const float lfoTarget = *valueTreeState.getRawParameterValue("lfoTarget");
    
    // Apply LFO modulation to cutoff if targeted
    float modulatedCutoff = cutoff;
    if (lfoTarget < 0.5f && lfoDepth > 0.0f) // Target is cutoff
    {
        float lfoValue = getLFOValue();
        modulatedCutoff = juce::jlimit(20.0f, 20000.0f, 
            cutoff + (lfoValue * lfoDepth * 1000.0f));
    }
    
    // Set filter parameters
    auto cutoffHz = juce::jmap(modulatedCutoff, 0.0f, 100.0f, 20.0f, 20000.0f);
    auto q = juce::jmap(resonance, 0.0f, 100.0f, 0.5f, 10.0f);
    
    // Set filter type
    const int mode = (int) std::round(*valueTreeState.getRawParameterValue("filterMode"));

    switch (mode) // 0=LP, 1=BP, 2=HP mapping you already have
{
    case 0: stateVariableFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);  break;
    case 1: stateVariableFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass); break;
    case 2: stateVariableFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass); break;
    default: break;
}
    
    stateVariableFilter.setCutoffFrequency(cutoffHz);
    stateVariableFilter.setResonance(q);
    
    // Process
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    stateVariableFilter.process(context);
}

void MyPluginAudioProcessor::processPitchShift(juce::AudioBuffer<float>& buffer)
{
    const float semitones = *valueTreeState.getRawParameterValue("pitchSemitones");
    const float octaves = *valueTreeState.getRawParameterValue("pitchOctaves");
    
    float totalSemitones = semitones + (octaves * 12.0f);
    
    if (std::abs(totalSemitones) < 0.1f) return; // No pitch shifting needed
    
    float pitchRatio = std::pow(2.0f, totalSemitones / 12.0f);
    
    // Update pitch smoothers
    for (auto& smoother : pitchSmoother)
        smoother.setTargetValue(pitchRatio);
    
    // Simple granular pitch shifting
    const int grainSize = 1024;
    const int overlap = grainSize / 2;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* pitchBufferData = pitchBuffer[channel].data();
        float currentPitchRatio = pitchSmoother[channel].getTargetValue();
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Write input to pitch buffer
            pitchBufferData[pitchWriteIndex[channel]] = channelData[sample];
            
            // Read with pitch shift (simple time-domain approach)
            if (pitchWriteIndex[channel] > grainSize)
            {
                float readPos = pitchWriteIndex[channel] - grainSize * (1.0f / currentPitchRatio);
                int readIndex = static_cast<int>(readPos) % pitchBuffer[channel].size();
                
                if (readIndex >= 0 && readIndex < pitchBuffer[channel].size())
                {
                    // Simple linear interpolation
                    float fraction = readPos - static_cast<int>(readPos);
                    int nextIndex = (readIndex + 1) % pitchBuffer[channel].size();
                    
                    float interpolatedSample = pitchBufferData[readIndex] * (1.0f - fraction) + 
                                             pitchBufferData[nextIndex] * fraction;
                    
                    // Apply window to reduce artifacts
                    float window = 0.5f + 0.5f * std::cos(juce::MathConstants<float>::pi * 
                                   (sample % grainSize) / float(grainSize));
                    
                    channelData[sample] = interpolatedSample * window * 0.5f + channelData[sample] * 0.5f;
                }
            }
            
            pitchWriteIndex[channel] = (pitchWriteIndex[channel] + 1) % pitchBuffer[channel].size();
        }
    }
}

void MyPluginAudioProcessor::processChorus(juce::AudioBuffer<float>& buffer)
{
    const float chorusRate = *valueTreeState.getRawParameterValue("chorusRate");
    const float chorusDepth = *valueTreeState.getRawParameterValue("chorusDepth");
    const float chorusMix = *valueTreeState.getRawParameterValue("chorusMix") / 100.0f;
    
    if (chorusMix > 0.0f)
    {
        chorus.setRate(juce::jmap(chorusRate, 0.0f, 100.0f, 0.1f, 5.0f));
        chorus.setDepth(juce::jmap(chorusDepth, 0.0f, 100.0f, 0.0f, 1.0f));
        chorus.setCentreDelay(5.0f); // 5ms center delay
        chorus.setFeedback(0.3f);
        chorus.setMix(chorusMix);
        
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        chorus.process(context);
    }
}

void MyPluginAudioProcessor::processFlanger(juce::AudioBuffer<float>& buffer)
{
    // Read UI parameters
    const float flangerDelayParam    = *valueTreeState.getRawParameterValue("flangerDelay");     // 0..100
    const float flangerDepthParam    = *valueTreeState.getRawParameterValue("flangerDepth");     // 0..100
    const float flangerRateParam     = *valueTreeState.getRawParameterValue("flangerRate");      // 0..100
    const float flangerMixParam      = *valueTreeState.getRawParameterValue("flangerMix") / 100.0f; // 0..1
    const float flangerFeedbackParam = *valueTreeState.getRawParameterValue("flangerFeedback");  // 0..100

    // Smooth feedback (single smoother, no per-channel array)
    flangerFeedbackSmoother.setTargetValue(flangerFeedbackParam / 100.0f);
    const float fb = flangerFeedbackSmoother.getNextValue();

    // Map your UI ranges to Chorus/Flanger settings (tweak to taste)
    const float centreMs = juce::jmap(flangerDelayParam, 0.0f, 100.0f, 0.1f, 10.0f); // ~0.1–10 ms
    const float depth    = juce::jmap(flangerDepthParam, 0.0f, 100.0f, 0.0f, 5.0f);  // modulation depth (ms)
    const float rateHz   = juce::jmap(flangerRateParam,  0.0f, 100.0f, 0.10f, 2.0f); // LFO rate

    flanger.setCentreDelay(centreMs);
    flanger.setDepth(depth);
    flanger.setRate(rateHz);
    flanger.setFeedback(fb);
    flanger.setMix(flangerMixParam);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    flanger.process(ctx); // applies flanger in-place to the current buffer
}

void MyPluginAudioProcessor::processPanning(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2) return;
    
    const float panValue = *valueTreeState.getRawParameterValue("panPosition");
    const float lfoDepth = *valueTreeState.getRawParameterValue("lfoDepth");
    const float lfoTarget = *valueTreeState.getRawParameterValue("lfoTarget");
    
    // Apply LFO modulation to pan if targeted
    float modulatedPan = panValue;
    if (lfoTarget > 0.5f && lfoDepth > 0.0f) // Target is pan
    {
        float lfoValue = getLFOValue();
        modulatedPan = juce::jlimit(-100.0f, 100.0f, panValue + (lfoValue * lfoDepth));
    }
    
    // Update pan smoothers
    for (auto& smoother : panSmoother)
        smoother.setTargetValue(modulatedPan / 100.0f); // Normalize to -1 to 1
    
    // Apply equal-power panning
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float panPos = panSmoother[0].getNextValue(); // Use same pan for both channels
        
        // Equal power panning
        float panAngle = (panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        float leftGain = std::cos(panAngle);
        float rightGain = std::sin(panAngle);
        
        float leftSample = buffer.getSample(0, sample);
        float rightSample = buffer.getSample(1, sample);
        
        // Mix and apply gains
        float mixedSample = (leftSample + rightSample) * 0.5f;
        
        buffer.setSample(0, sample, mixedSample * leftGain);
        buffer.setSample(1, sample, mixedSample * rightGain);
    }
}

void MyPluginAudioProcessor::captureWaveformData(const juce::AudioBuffer<float>& buffer)
{
    if (++waveformDownsampleCounter >= waveformDownsampleRate)
    {
        waveformDownsampleCounter = 0;
        
        // Calculate RMS for current buffer
        float rms = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float channelRms = buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
            rms += channelRms * channelRms;
        }
        rms = std::sqrt(rms / buffer.getNumChannels());
        
        // Store in circular buffer
        int writeIdx = waveformData.writeIndex.load();
        waveformData.buffer[writeIdx] = rms;
        waveformData.writeIndex = (writeIdx + 1) % waveformData.buffer.size();
        waveformData.hasNewData = true;
    }
}

// === Original Helper Functions ===

void MyPluginAudioProcessor::triggerNewGrain(int channel, int grainSize, float spray, bool reverse, float randomization)
{
    for (int i = 0; i < maxGrains; ++i)
    {
        if (!activeGrains[channel][i].isActive)
        {
            auto& grain = activeGrains[channel][i];

            const float sprayAmount = spray * (random.nextFloat() * 2.0f - 1.0f);
            const int sprayOffset = static_cast<int>(grainSize * sprayAmount);

            grain.startPos = (delayWriteIndex[channel] - grainSize + sprayOffset + maxDelayTime) % maxDelayTime;
            grain.size = juce::jlimit(32, 16384, grainSize);
            grain.position = 0;
            grain.isActive = true;
            grain.isReverse = reverse;
            grain.amplitude = 1.0f;

            if (randomization > 0.0f)
            {
                const float sizeVariation = 1.0f + (random.nextFloat() * 2.0f - 1.0f) * randomization * 0.5f;
                grain.size = juce::jlimit(32, 16384, static_cast<int>(grain.size * sizeVariation));
                grain.amplitude *= (1.0f + (random.nextFloat() * 2.0f - 1.0f) * randomization * 0.3f);
            }
            break;
        }
    }
}

float MyPluginAudioProcessor::processActiveGrains(int channel, float* delayBuffer)
{
    float output = 0.0f;

    for (int i = 0; i < maxGrains; ++i)
    {
        auto& g = activeGrains[channel][i];
        if (!g.isActive) continue;

        const float progress = static_cast<float>(g.position) / juce::jmax(1, g.size);
        const float envelope = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * progress));

        int readPos = g.isReverse
            ? (g.startPos + g.size - g.position + maxDelayTime) % maxDelayTime
            : (g.startPos + g.position) % maxDelayTime;

        output += delayBuffer[readPos] * envelope * g.amplitude;

        if (++g.position >= g.size)
            g.isActive = false;
    }

    return output;
}

float MyPluginAudioProcessor::applyEQFiltering(float sample, int channel, float highCut, float lowCut)
{
    const float highCutFreq = juce::jmap(highCut, 0.0f, 100.0f, 200.0f, 20000.0f);
    const float lowCutFreq = juce::jmap(lowCut, 0.0f, 100.0f, 10.0f, 1000.0f);

    const float lpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * (highCutFreq / (float)getSampleRate()));
    highCutState[channel] = lpCoeff * highCutState[channel] + (1.0f - lpCoeff) * sample;

    const float hpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * (lowCutFreq / (float)getSampleRate()));
    lowCutState[channel] = hpCoeff * lowCutState[channel] + (1.0f - hpCoeff) * highCutState[channel];

    return highCutState[channel] - lowCutState[channel];
}

void MyPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = valueTreeState.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MyPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr)
        if (xmlState->hasTagName(valueTreeState.state.getType()))
            valueTreeState.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyPluginAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout MyPluginAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // === ORIGINAL PARAMETERS ===
    params.push_back(std::make_unique<P>("delayTime", "Delay Time (ms)",
        juce::NormalisableRange<float>(1.f, 2000.f, 0.01f, 0.35f), 400.f));
    params.push_back(std::make_unique<P>("feedback", "Feedback",
        juce::NormalisableRange<float>(0.f, 0.95f, 0.001f, 0.5f), 0.35f));
    params.push_back(std::make_unique<P>("mix", "Mix (%)",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 35.f));

    params.push_back(std::make_unique<P>("grainSize", "Grain Size (ms)",
        juce::NormalisableRange<float>(5.f, 200.f, 0.01f, 0.5f), 60.f));
    params.push_back(std::make_unique<P>("grainDensity", "Grain Density",
        juce::NormalisableRange<float>(0.1f, 4.0f, 0.001f, 0.5f), 1.0f));
    params.push_back(std::make_unique<P>("grainSpray", "Grain Spray (%)",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 10.f));
    params.push_back(std::make_unique<B>("reverseGrains", "Reverse Grains", false));
    params.push_back(std::make_unique<P>("randomization", "Randomization (%)",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 15.f));

    params.push_back(std::make_unique<P>("stereoWidth", "Stereo Width (%)",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 50.f));
    params.push_back(std::make_unique<P>("eqHigh", "High Cut",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 1.0f), 80.f));
    params.push_back(std::make_unique<P>("eqLow", "Low Cut",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 1.0f), 10.f));

    // === NEW ADVANCED PARAMETERS ===
    
    // Filter
    params.push_back(std::make_unique<P>("filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.3f), 70.f));
    params.push_back(std::make_unique<P>("filterResonance", "Filter Resonance",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 10.f));
    params.push_back(std::make_unique<P>("filterType", "Filter Type",
        juce::NormalisableRange<float>(0.f, 100.f, 1.f), 0.f)); // 0-33=LP, 34-66=BP, 67-100=HP
    
    // Pitch
    params.push_back(std::make_unique<P>("pitchSemitones", "Pitch Semitones",
        juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    params.push_back(std::make_unique<P>("pitchOctaves", "Pitch Octaves",
        juce::NormalisableRange<float>(-2.f, 2.f, 0.01f), 0.f));
    
    // Pan
    params.push_back(std::make_unique<P>("panPosition", "Pan Position",
        juce::NormalisableRange<float>(-100.f, 100.f, 0.01f), 0.f));
    
    // LFO
    params.push_back(std::make_unique<P>("lfoRate", "LFO Rate",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 25.f));
    params.push_back(std::make_unique<P>("lfoDepth", "LFO Depth",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 0.f));
    params.push_back(std::make_unique<P>("lfoTarget", "LFO Target",
        juce::NormalisableRange<float>(0.f, 100.f, 1.f), 0.f)); // 0-50=Cutoff, 51-100=Pan
    params.push_back(std::make_unique<B>("lfoBipolar", "LFO Bipolar", true));
    params.push_back(std::make_unique<P>("lfoWaveform", "LFO Waveform",
        juce::NormalisableRange<float>(0.f, 100.f, 1.f), 0.f)); // 0-50=Sine, 51-100=Triangle
    params.push_back(std::make_unique<B>("lfoTempoSync", "LFO Tempo Sync", false));
    params.push_back(std::make_unique<P>("lfoSyncDivision", "LFO Sync Division",
        juce::NormalisableRange<float>(0.f, 2.f, 1.f), 2.f)); // 0=1/4, 1=1/2, 2=1bar
    
    // Chorus
    params.push_back(std::make_unique<P>("chorusRate", "Chorus Rate",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 50.f));
    params.push_back(std::make_unique<P>("chorusDepth", "Chorus Depth",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 30.f));
    params.push_back(std::make_unique<P>("chorusMix", "Chorus Mix",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 0.f));
    
    // Flanger
    params.push_back(std::make_unique<P>("flangerDelay", "Flanger Delay",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 25.f));
    params.push_back(std::make_unique<P>("flangerFeedback", "Flanger Feedback",
        juce::NormalisableRange<float>(0.f, 95.f, 0.01f, 0.5f), 40.f));
    params.push_back(std::make_unique<P>("flangerDepth", "Flanger Depth",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 60.f));
    params.push_back(std::make_unique<P>("flangerRate", "Flanger Rate",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 30.f));
    params.push_back(std::make_unique<P>("flangerMix", "Flanger Mix",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f, 0.5f), 0.f));

    return {params.begin(), params.end()};
}