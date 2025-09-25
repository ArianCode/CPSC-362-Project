#include "PluginEditor.h"
#include "PluginProcessor.h"

// ================================================================================
// WaterfallLookAndFeel Implementation
// ================================================================================

WaterfallLookAndFeel::WaterfallLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, backgroundDark);
    setColour(juce::Slider::thumbColourId, waterfallPrimary);
    setColour(juce::Slider::rotarySliderFillColourId, waterfallPrimary);
    setColour(juce::Slider::rotarySliderOutlineColourId, darkGreen);
}



void WaterfallLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(6);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto cx = bounds.getCentreX(), cy = bounds.getCentreY();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Face
    g.setColour(juce::Colour(0xff111827));
    g.fillEllipse(bounds);

    // Value arc
    juce::Path arc;
    arc.addCentredArc(cx, cy, radius - 4, radius - 4, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(waterfallPrimary.withAlpha(0.3f));
    g.strokePath(arc, juce::PathStrokeType(6.f));
    g.setColour(waterfallPrimary);
    g.strokePath(arc, juce::PathStrokeType(3.f));

    // Needle
    juce::Path needle; needle.addRectangle(-1.5f, -radius * 0.6f, 3.f, radius * 0.35f);
    g.setColour(juce::Colours::white);
    g.fillPath(needle, juce::AffineTransform::rotation(angle).translated(cx, cy));
}


void WaterfallLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                            const juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/)
{
    auto track = juce::Rectangle<float>(x, y, width, height).reduced(0, height / 3.0f);
    g.setColour(juce::Colour(0xff1f2937));
    g.fillRoundedRectangle(track, 3.0f);

    auto filled = track.withWidth(track.getWidth() * sliderPos);
    g.setColour(waterfallPrimary);
    g.fillRoundedRectangle(filled, 3.0f);
}


void WaterfallLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                        int /*buttonX*/, int /*buttonY*/, int /*buttonWidth*/, int /*buttonHeight*/,
                                        juce::ComboBox& /*box*/)
{
    auto r = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(juce::Colour(0x80000000));
    g.fillRoundedRectangle(r, 20.0f);
    g.setColour(waterfallPrimary.withAlpha(0.4f));
    g.drawRoundedRectangle(r, 20.0f, 1.0f);
}


void WaterfallLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    
    auto baseColour = shouldDrawButtonAsHighlighted ? waterfallPrimary.withAlpha(0.3f) : backgroundDark.withAlpha(0.8f);
    
    if (shouldDrawButtonAsDown)
        baseColour = waterfallSecondary.withAlpha(0.5f);
    
    // Cosmic glow effect for interaction
    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour(waterfallPrimary.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.expanded(2), bounds.getHeight() * 0.6f);
    }
    
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
    
    // Cosmic border
    juce::ColourGradient borderGradient(
        waterfallPrimary.withAlpha(shouldDrawButtonAsHighlighted ? 0.8f : 0.4f), 
        bounds.getX(), bounds.getY(),
        waterfallSecondary.withAlpha(shouldDrawButtonAsHighlighted ? 0.6f : 0.3f), 
        bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(borderGradient);
    g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.5f, 2.0f);
}

// ================================================================================
// CustomKnob Implementation (unchanged)
// ================================================================================

CustomKnob::CustomKnob(juce::AudioProcessorValueTreeState& vts, const juce::String& paramID,
                       const juce::String& labelText, const juce::String& tooltipText)
    : tooltip(tooltipText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);
    
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffb19cd9)); // Cosmic purple text
    addAndMakeVisible(label);
    
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, slider);
    
    slider.onValueChange = [this]() {
        if (onHover)
        {
            auto value = slider.getValue();
            auto paramName = label.getText();
            onHover(paramName + ": " + juce::String(value, 2));
        }
    };
}

CustomKnob::~CustomKnob() = default;

void CustomKnob::paint(juce::Graphics& g) {}

void CustomKnob::resized()
{
    auto bounds = getLocalBounds();
    slider.setBounds(bounds.removeFromTop(bounds.getHeight() - 20));
    label.setBounds(bounds);
}

void CustomKnob::mouseEnter(const juce::MouseEvent& event)
{
    if (onHover)
        onHover(tooltip);
}

void CustomKnob::mouseExit(const juce::MouseEvent& event)
{
    if (onHover)
        onHover("Hover over controls for parameter info");
}

// ================================================================================
// WaveformDisplay Implementation
// ================================================================================

WaveformDisplay::WaveformDisplay(MyPluginAudioProcessor& processor)
    : audioProcessor(processor)
{
    startTimerHz(60); // 60 FPS for smooth animation
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Background
    g.setColour(juce::Colour(0x80000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    // Border
    g.setColour(juce::Colour(0x4064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 2.0f);
    
    // Title
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(14.0f);
    auto titleArea = bounds.removeFromTop(25);
    g.drawText("Real-time Waveform", titleArea, juce::Justification::centred);
    
    // Waveform
    bounds.reduce(10, 5);
    
    if (!waveformPath.isEmpty())
    {
        // Gradient fill
        juce::ColourGradient waveGradient(
            juce::Colour(0xff64c896), bounds.getX(), bounds.getCentreY(),
            juce::Colour(0xff4a90e2), bounds.getRight(), bounds.getCentreY(), false);
        g.setGradientFill(waveGradient);
        g.strokePath(waveformPath, juce::PathStrokeType(2.0f));
        
        // Glow effect
        g.setColour(juce::Colour(0x4064c896));
        g.strokePath(waveformPath, juce::PathStrokeType(4.0f));
    }
    else
    {
        // No signal indicator
        g.setColour(juce::Colour(0x8064c896));
        g.setFont(12.0f);
        g.drawText("No signal", bounds, juce::Justification::centred);
    }
}

void WaveformDisplay::timerCallback()
{
    if (audioProcessor.waveformData.hasNewData.load())
    {
        // Copy waveform data
        int readIndex = audioProcessor.waveformData.writeIndex.load();
        for (int i = 0; i < displayBuffer.size(); ++i)
        {
            int sourceIndex = (readIndex - displayBuffer.size() + i + audioProcessor.waveformData.buffer.size()) 
                             % audioProcessor.waveformData.buffer.size();
            displayBuffer[i] = audioProcessor.waveformData.buffer[sourceIndex];
        }
        
        // Create waveform path
        waveformPath.clear();
        auto bounds = getLocalBounds().reduced(10, 30).reduced(10, 5);
        
        if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
        {
            float maxValue = 0.001f; // Avoid division by zero
            for (float value : displayBuffer)
                maxValue = juce::jmax(maxValue, std::abs(value));
            
            bool hasStarted = false;
            for (int i = 0; i < displayBuffer.size(); ++i)
            {
                float x = juce::jmap(float(i), 0.0f, float(displayBuffer.size() - 1), 
                                   float(bounds.getX()), float(bounds.getRight()));
                float y = bounds.getCentreY() - (displayBuffer[i] / maxValue) * bounds.getHeight() * 0.4f;
                
                if (!hasStarted)
                {
                    waveformPath.startNewSubPath(x, y);
                    hasStarted = true;
                }
                else
                {
                    waveformPath.lineTo(x, y);
                }
            }
        }
        
        audioProcessor.waveformData.hasNewData = false;
        repaint();
    }
}

// ================================================================================
// Filter Section Implementation
// ================================================================================

FilterSection::FilterSection(juce::AudioProcessorValueTreeState& vts)
    : valueTreeState(vts)
{
    cutoffKnob = std::make_unique<CustomKnob>(vts, "filterCutoff", "Cutoff", 
        "Filter Cutoff → Controls the filter frequency (20Hz-20kHz)");
    addAndMakeVisible(*cutoffKnob);
    
    resonanceKnob = std::make_unique<CustomKnob>(vts, "filterResonance", "Resonance", 
        "Filter Resonance → Controls filter resonance/Q factor");
    addAndMakeVisible(*resonanceKnob);
    
    filterTypeBox.addItem("Low Pass", 1);
    filterTypeBox.addItem("Band Pass", 2);
    filterTypeBox.addItem("High Pass", 3);
    filterTypeBox.setSelectedId(1);
    addAndMakeVisible(filterTypeBox);
    
    filterTypeLabel.setText("Type", juce::dontSendNotification);
    filterTypeLabel.setJustificationType(juce::Justification::centred);
    filterTypeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0c0a0));
    addAndMakeVisible(filterTypeLabel);
    
    // Custom callback to handle filter type changes
    filterTypeBox.onChange = [this]() {
        if (auto* param = valueTreeState.getParameter("filterType"))
        {
            float value = 0.0f;
            switch (filterTypeBox.getSelectedId())
            {
                case 1: value = 0.0f; break;   // Low Pass (0-33)
                case 2: value = 50.0f; break;  // Band Pass (34-66) 
                case 3: value = 100.0f; break; // High Pass (67-100)
            }
            param->setValueNotifyingHost(value / 100.0f);
        }
    };
}

FilterSection::~FilterSection() = default;

void FilterSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Section background
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    // Border
    g.setColour(juce::Colour(0x6064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 2.0f);
    
    // Title
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(16.0f);
    g.drawText("FILTER", bounds.removeFromTop(25), juce::Justification::centred);
}

void FilterSection::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30); // Space for title
    bounds.reduce(10, 5);
    
    auto knobArea = bounds.removeFromTop(bounds.getHeight() * 0.7f);
    cutoffKnob->setBounds(knobArea.removeFromLeft(knobArea.getWidth() / 2).reduced(5));
    resonanceKnob->setBounds(knobArea.reduced(5));
    
    bounds.removeFromTop(5);
    filterTypeLabel.setBounds(bounds.removeFromTop(15));
    filterTypeBox.setBounds(bounds.reduced(5));
}

// ================================================================================
// Pitch Section Implementation  
// ================================================================================

PitchSection::PitchSection(juce::AudioProcessorValueTreeState& vts)
{
    semitonesKnob = std::make_unique<CustomKnob>(vts, "pitchSemitones", "Semitones", 
        "Pitch Semitones → Fine pitch adjustment (-12 to +12 semitones)");
    addAndMakeVisible(*semitonesKnob);
    
    octavesKnob = std::make_unique<CustomKnob>(vts, "pitchOctaves", "Octaves", 
        "Pitch Octaves → Coarse pitch adjustment (-2 to +2 octaves)");
    addAndMakeVisible(*octavesKnob);
}

PitchSection::~PitchSection() = default;

void PitchSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    g.setColour(juce::Colour(0x6064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 2.0f);
    
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(16.0f);
    g.drawText("PITCH", bounds.removeFromTop(25), juce::Justification::centred);
}

void PitchSection::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);
    bounds.reduce(10, 5);
    
    semitonesKnob->setBounds(bounds.removeFromTop(bounds.getHeight() / 2).reduced(5));
    octavesKnob->setBounds(bounds.reduced(5));
}

// ================================================================================
// LFO & Pan Section Implementation
// ================================================================================

LfoPanSection::LfoPanSection(juce::AudioProcessorValueTreeState& vts)
{
    // LFO knobs
    lfoRateKnob = std::make_unique<CustomKnob>(vts, "lfoRate", "Rate", 
        "LFO Rate → Controls LFO speed (0.1Hz - 10Hz)");
    addAndMakeVisible(*lfoRateKnob);
    
    lfoDepthKnob = std::make_unique<CustomKnob>(vts, "lfoDepth", "Depth", 
        "LFO Depth → Controls modulation amount");
    addAndMakeVisible(*lfoDepthKnob);
    
    // LFO target
    lfoTargetBox.addItem("Filter Cutoff", 1);
    lfoTargetBox.addItem("Pan Position", 2);
    lfoTargetBox.setSelectedId(1);
    addAndMakeVisible(lfoTargetBox);
    lfoTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        vts, "lfoTarget", lfoTargetBox);
    
    // LFO waveform
    lfoWaveformBox.addItem("Sine", 1);
    lfoWaveformBox.addItem("Triangle", 2);
    lfoWaveformBox.setSelectedId(1);
    addAndMakeVisible(lfoWaveformBox);
    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        vts, "lfoWaveform", lfoWaveformBox);
    
    // LFO buttons
    lfoBipolarButton.setButtonText("Bipolar");
    addAndMakeVisible(lfoBipolarButton);
    lfoBipolarAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        vts, "lfoBipolar", lfoBipolarButton);
    
    lfoSyncButton.setButtonText("Sync");
    addAndMakeVisible(lfoSyncButton);
    lfoSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        vts, "lfoTempoSync", lfoSyncButton);
    
    // LFO sync division
    lfoSyncDivisionBox.addItem("1/4 Note", 1);
    lfoSyncDivisionBox.addItem("1/2 Note", 2);
    lfoSyncDivisionBox.addItem("1 Bar", 3);
    lfoSyncDivisionBox.setSelectedId(3);
    addAndMakeVisible(lfoSyncDivisionBox);
    lfoSyncDivisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        vts, "lfoSyncDivision", lfoSyncDivisionBox);
    
    // Pan knob
    panKnob = std::make_unique<CustomKnob>(vts, "panPosition", "Pan", 
        "Pan Position → Controls stereo positioning (-100% Left to +100% Right)");
    addAndMakeVisible(*panKnob);
}

LfoPanSection::~LfoPanSection() = default;

void LfoPanSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    g.setColour(juce::Colour(0x6064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 2.0f);
    
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(16.0f);
    g.drawText("LFO & PAN", bounds.removeFromTop(25), juce::Justification::centred);
}

void LfoPanSection::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);
    bounds.reduce(8, 5);
    
    // LFO knobs row
    auto knobRow = bounds.removeFromTop(80);
    lfoRateKnob->setBounds(knobRow.removeFromLeft(knobRow.getWidth() / 3).reduced(3));
    lfoDepthKnob->setBounds(knobRow.removeFromLeft(knobRow.getWidth() / 2).reduced(3));
    panKnob->setBounds(knobRow.reduced(3));
    
    bounds.removeFromTop(5);
    
    // Target and waveform row
    auto comboRow = bounds.removeFromTop(25);
    lfoTargetBox.setBounds(comboRow.removeFromLeft(comboRow.getWidth() / 2).reduced(2));
    lfoWaveformBox.setBounds(comboRow.reduced(2));
    
    bounds.removeFromTop(5);
    
    // Button row
    auto buttonRow = bounds.removeFromTop(25);
    lfoBipolarButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 3).reduced(2));
    lfoSyncButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(2));
    lfoSyncDivisionBox.setBounds(buttonRow.reduced(2));
}

// ================================================================================
// Chorus Section Implementation
// ================================================================================

ChorusSection::ChorusSection(juce::AudioProcessorValueTreeState& vts)
{
    rateKnob = std::make_unique<CustomKnob>(vts, "chorusRate", "Rate", 
        "Chorus Rate → Controls chorus modulation speed");
    addAndMakeVisible(*rateKnob);
    
    depthKnob = std::make_unique<CustomKnob>(vts, "chorusDepth", "Depth", 
        "Chorus Depth → Controls chorus modulation depth");
    addAndMakeVisible(*depthKnob);
    
    mixKnob = std::make_unique<CustomKnob>(vts, "chorusMix", "Mix", 
        "Chorus Mix → Controls wet/dry balance for chorus effect");
    addAndMakeVisible(*mixKnob);
}

ChorusSection::~ChorusSection() = default;

void ChorusSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    g.setColour(juce::Colour(0x6064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 2.0f);
    
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(16.0f);
    g.drawText("CHORUS", bounds.removeFromTop(25), juce::Justification::centred);
}

void ChorusSection::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);
    bounds.reduce(10, 5);
    
    auto knobWidth = bounds.getWidth() / 3;
    rateKnob->setBounds(bounds.removeFromLeft(knobWidth).reduced(3));
    depthKnob->setBounds(bounds.removeFromLeft(knobWidth).reduced(3));
    mixKnob->setBounds(bounds.reduced(3));
}

// ================================================================================
// Flanger Section Implementation
// ================================================================================

FlangerSection::FlangerSection(juce::AudioProcessorValueTreeState& vts)
{
    delayKnob = std::make_unique<CustomKnob>(vts, "flangerDelay", "Delay", 
        "Flanger Delay → Base delay time for flanger effect");
    addAndMakeVisible(*delayKnob);
    
    feedbackKnob = std::make_unique<CustomKnob>(vts, "flangerFeedback", "Feedback", 
        "Flanger Feedback → Controls feedback amount for resonance");
    addAndMakeVisible(*feedbackKnob);
    
    depthKnob = std::make_unique<CustomKnob>(vts, "flangerDepth", "Depth", 
        "Flanger Depth → Controls modulation depth of delay time");
    addAndMakeVisible(*depthKnob);
    
    rateKnob = std::make_unique<CustomKnob>(vts, "flangerRate", "Rate", 
        "Flanger Rate → Controls LFO speed for delay modulation");
    addAndMakeVisible(*rateKnob);
    
    mixKnob = std::make_unique<CustomKnob>(vts, "flangerMix", "Mix", 
        "Flanger Mix → Controls wet/dry balance for flanger effect");
    addAndMakeVisible(*mixKnob);
}

FlangerSection::~FlangerSection() = default;

void FlangerSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    g.setColour(juce::Colour(0x6064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 2.0f);
    
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(16.0f);
    g.drawText("FLANGER", bounds.removeFromTop(25), juce::Justification::centred);
}

void FlangerSection::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);
    bounds.reduce(8, 5);
    
    // Top row: 3 knobs
    auto topRow = bounds.removeFromTop(bounds.getHeight() / 2);
    auto knobWidth = topRow.getWidth() / 3;
    delayKnob->setBounds(topRow.removeFromLeft(knobWidth).reduced(2));
    feedbackKnob->setBounds(topRow.removeFromLeft(knobWidth).reduced(2));
    depthKnob->setBounds(topRow.reduced(2));
    
    bounds.removeFromTop(5);
    
    // Bottom row: 2 knobs
    auto knobWidth2 = bounds.getWidth() / 2;
    rateKnob->setBounds(bounds.removeFromLeft(knobWidth2).reduced(2));
    mixKnob->setBounds(bounds.reduced(2));
}

// ================================================================================
// Main Tab Component Implementation
// ================================================================================

MainTabComponent::MainTabComponent(MyPluginAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupControls();
    setupSections();
    startTimerHz(30);
}

MainTabComponent::~MainTabComponent()
{
    stopTimer();
}

void MainTabComponent::setupControls()
{
    // Preset selector
    presetSelector = std::make_unique<PresetComboBox>(audioProcessor.valueTreeState);
    presetSelector->onPresetLoaded = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
    addAndMakeVisible(*presetSelector);
    
    // Randomize button
    randomizeBtn = std::make_unique<RandomizeButton>(audioProcessor.valueTreeState);
    randomizeBtn->onRandomize = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
    addAndMakeVisible(*randomizeBtn);
    
    // Mix slider
    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(mixSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.valueTreeState, "mix", mixSlider);
    
    // Grain visualizer
    grainViz = std::make_unique<GrainVisualizer>();
    addAndMakeVisible(*grainViz);
}

void MainTabComponent::setupSections()
{
    auto createKnob = [this](const juce::String& paramID, const juce::String& label, const juce::String& tooltip) {
        auto knob = std::make_unique<CustomKnob>(audioProcessor.valueTreeState, paramID, label, tooltip);
        addAndMakeVisible(*knob);
        return knob;
    };
    
    // Delay section
    delayKnobs.push_back(createKnob("delayTime", "Time", "Delay Time → Controls echo spacing (ms/note values)"));
    delayKnobs.push_back(createKnob("feedback", "Feedback", "Feedback → Amount of signal fed back for echoes"));
    delayKnobs.push_back(createKnob("stereoWidth", "Stereo", "Stereo Width → Controls ping-pong spread"));
    
    // EQ section
    eqKnobs.push_back(createKnob("eqHigh", "High", "High Cut → Removes high frequencies from delay"));
    eqKnobs.push_back(createKnob("eqLow", "Low", "Low Cut → Removes low frequencies from delay"));
    
    // Granular section
    granularKnobs.push_back(createKnob("grainSize", "Grain Size", "Grain Size → Short = glitchy, Long = smooth"));
    granularKnobs.push_back(createKnob("grainDensity", "Density", "Grain Density → Controls grain overlap & thickness"));
    granularKnobs.push_back(createKnob("grainSpray", "Spray", "Grain Spray → Randomizes grain timing & pitch"));
    
    // Creative section
    creativeKnobs.push_back(createKnob("randomization", "Randomization", "Randomization → Adds controlled chaos to parameters"));
}

void MainTabComponent::paint(juce::Graphics& g)
{
    drawWaterfallBackground(g);
}

void MainTabComponent::drawWaterfallBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Deep space background
    g.fillAll(juce::Colour(0xff0b0c10));
    
    // Cosmic nebula gradient
    juce::ColourGradient nebulaGradient(
        juce::Colour(0x309d4edd), bounds.getX(), bounds.getY(),
        juce::Colour(0x20e63946), bounds.getRight(), bounds.getBottom(), false);
    nebulaGradient.addColour(0.2, juce::Colour(0x253d5a80));
    nebulaGradient.addColour(0.5, juce::Colour(0x40f72585));
    nebulaGradient.addColour(0.8, juce::Colour(0x1500d9ff));
    g.setGradientFill(nebulaGradient);
    g.fillAll();
    
    // Cosmic streak lines across the background
    drawCosmicStreaks(g, bounds);
    
    // Branded cosmic watermark
    g.setColour(juce::Colour(0x409d4edd));
    g.setFont(11.0f);
    auto watermarkBounds = bounds.removeFromBottom(30).removeFromRight(250);
    g.drawText("𐌔𐌵𐌐𐌄𐌓 𐌔𐌀𐌵𐌂𐌄 𐌃𐌄𐌋𐌀𐌙 by 𐌀𐌓𐌉𐌀𐌍 𐌇𐋅𐌏𐌋𐌀𐌌𐌉𐌐𐌏𐌵𐌓", watermarkBounds, juce::Justification::centredRight);
}

void MainTabComponent::drawCosmicStreaks(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::Random random(123); // Different seed for variety
    
    // Draw cosmic streak lines
    for (int i = 0; i < 20; ++i)
    {
        float startX = random.nextFloat() * bounds.getWidth();
        float startY = random.nextFloat() * bounds.getHeight();
        float length = 50 + random.nextFloat() * 150;
        float angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        float endX = startX + std::cos(angle) * length;
        float endY = startY + std::sin(angle) * length;
        
        // Cosmic color palette
        juce::Colour streakColor;
        switch (i % 6)
        {
            case 0: streakColor = juce::Colour(0x359d4edd); break; // Purple
            case 1: streakColor = juce::Colour(0x25e63946); break; // Red
            case 2: streakColor = juce::Colour(0x303d5a80); break; // Blue
            case 3: streakColor = juce::Colour(0x40f72585); break; // Pink
            case 4: streakColor = juce::Colour(0x2000d9ff); break; // Cyan
            case 5: streakColor = juce::Colour(0x30ff6b35); break; // Orange
        }
        
        juce::ColourGradient streakGradient(
            streakColor, startX, startY,
            streakColor.withAlpha(0.0f), endX, endY, false);
        g.setGradientFill(streakGradient);
        
        juce::Path streakPath;
        streakPath.startNewSubPath(startX, startY);
        streakPath.lineTo(endX, endY);
        g.strokePath(streakPath, juce::PathStrokeType(0.8f + random.nextFloat() * 2.0f));
        
        // Occasional sparkle points
        if (random.nextFloat() < 0.25f)
        {
            g.setColour(juce::Colour(0x8ff8f8ff));
            float sparkleX = startX + (endX - startX) * random.nextFloat();
            float sparkleY = startY + (endY - startY) * random.nextFloat();
            g.fillEllipse(sparkleX - 1.5f, sparkleY - 1.5f, 3, 3);
        }
    }
}

void MainTabComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Header (60px)
    auto headerArea = bounds.removeFromTop(60);
    auto headerMargin = 20;
    
    presetSelector->setBounds(headerArea.removeFromLeft(150).reduced(headerMargin, 15));
    randomizeBtn->setBounds(headerArea.removeFromRight(50).reduced(headerMargin, 15));
    
    // Control sections (300px)
    auto controlArea = bounds.removeFromTop(300);
    auto sectionWidth = controlArea.getWidth() / 4;
    auto margin = 15;
    
    // Delay section
    auto delayArea = controlArea.removeFromLeft(sectionWidth).reduced(margin);
    for (int i = 0; i < delayKnobs.size(); ++i)
    {
        auto knobArea = delayArea.removeFromTop(delayArea.getHeight() / delayKnobs.size());
        delayKnobs[i]->setBounds(knobArea.reduced(5));
    }
    
    // EQ section
    auto eqArea = controlArea.removeFromLeft(sectionWidth).reduced(margin);
    for (int i = 0; i < eqKnobs.size(); ++i)
    {
        auto knobArea = eqArea.removeFromTop(eqArea.getHeight() / eqKnobs.size());
        eqKnobs[i]->setBounds(knobArea.reduced(5));
    }
    
    // Granular section
    auto granularArea = controlArea.removeFromLeft(sectionWidth).reduced(margin);
    for (int i = 0; i < granularKnobs.size(); ++i)
    {
        auto knobArea = granularArea.removeFromTop(granularArea.getHeight() / granularKnobs.size());
        granularKnobs[i]->setBounds(knobArea.reduced(5));
    }
    
    // Creative section
    auto creativeArea = controlArea.reduced(margin);
    for (int i = 0; i < creativeKnobs.size(); ++i)
    {
        auto knobArea = creativeArea.removeFromTop(creativeArea.getHeight() / creativeKnobs.size());
        creativeKnobs[i]->setBounds(knobArea.reduced(5));
    }
    
    // Visualization area (150px)
    auto vizArea = bounds.removeFromTop(150).reduced(20);
    grainViz->setBounds(vizArea);
    
    // Footer with mix slider
    auto footerArea = bounds.reduced(100, 20);
    mixSlider.setBounds(footerArea);
}

void MainTabComponent::timerCallback()
{
    if (grainViz)
    {
        auto density = audioProcessor.valueTreeState.getRawParameterValue("grainDensity")->load();
        auto size = audioProcessor.valueTreeState.getRawParameterValue("grainSize")->load();
        auto reverse = audioProcessor.valueTreeState.getRawParameterValue("reverseGrains")->load() > 0.5f;
        
        grainViz->updateGrainActivity(density, size, reverse);
    }
}

// ================================================================================
// Advanced Tab Component Implementation
// ================================================================================

AdvancedTabComponent::AdvancedTabComponent(MyPluginAudioProcessor& processor)
    : audioProcessor(processor)
{
    // Create sections
    waveformDisplay = std::make_unique<WaveformDisplay>(processor);
    addAndMakeVisible(*waveformDisplay);
    
    filterSection = std::make_unique<FilterSection>(processor.valueTreeState);
    addAndMakeVisible(*filterSection);
    
    pitchSection = std::make_unique<PitchSection>(processor.valueTreeState);
    addAndMakeVisible(*pitchSection);
    
    lfoPanSection = std::make_unique<LfoPanSection>(processor.valueTreeState);
    addAndMakeVisible(*lfoPanSection);
    
    chorusSection = std::make_unique<ChorusSection>(processor.valueTreeState);
    addAndMakeVisible(*chorusSection);
    
    flangerSection = std::make_unique<FlangerSection>(processor.valueTreeState);
    addAndMakeVisible(*flangerSection);
    
    // Set up hover callbacks
    filterSection->onHover = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
    pitchSection->onHover = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
    lfoPanSection->onHover = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
    chorusSection->onHover = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
    flangerSection->onHover = [this](const juce::String& message) {
        if (onStatusUpdate) onStatusUpdate(message);
    };
}

AdvancedTabComponent::~AdvancedTabComponent() = default;

void AdvancedTabComponent::paint(juce::Graphics& g)
{
    drawWaterfallBackground(g);
}

void AdvancedTabComponent::drawWaterfallBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Deep space background
    g.fillAll(juce::Colour(0xff0b0c10));
    
    // Advanced cosmic gradient - different from main tab
    juce::ColourGradient advancedGradient(
        juce::Colour(0x403d5a80), bounds.getX(), bounds.getY(),
        juce::Colour(0x30f72585), bounds.getRight(), bounds.getBottom(), false);
    advancedGradient.addColour(0.3, juce::Colour(0x359d4edd));
    advancedGradient.addColour(0.6, juce::Colour(0x2500d9ff));
    advancedGradient.addColour(0.9, juce::Colour(0x25e63946));
    g.setGradientFill(advancedGradient);
    g.fillAll();
    
    // Advanced cosmic streaks - different pattern
    juce::Random random(456); // Different seed for advanced tab
    for (int i = 0; i < 12; ++i)
    {
        float startX = random.nextFloat() * bounds.getWidth();
        float startY = random.nextFloat() * bounds.getHeight();
        float endX = startX + (random.nextFloat() - 0.5f) * 300;
        float endY = startY + (random.nextFloat() - 0.5f) * 150;
        
        juce::Colour streakColor = juce::Colour(0x4000d9ff).withRotatedHue(random.nextFloat());
        
        juce::ColourGradient streakGradient(
            streakColor, startX, startY,
            streakColor.withAlpha(0.0f), endX, endY, false);
        g.setGradientFill(streakGradient);
        
        juce::Path streakPath;
        streakPath.startNewSubPath(startX, startY);
        streakPath.lineTo(endX, endY);
        g.strokePath(streakPath, juce::PathStrokeType(1.0f + random.nextFloat() * 1.5f));
    }
    
    // Cosmic watermark
    g.setColour(juce::Colour(0x6000d9ff));
    g.setFont(11.0f);
    auto watermarkBounds = bounds.removeFromBottom(30).removeFromRight(120);
    g.drawText("@𐌀𐌓𐌉𐌀𐌍._.𐌂", watermarkBounds, juce::Justification::centredRight);
}

void AdvancedTabComponent::resized()
{
    auto bounds = getLocalBounds().reduced(15, 10);
    
    // Row 1: Waveform (120px height)
    auto waveformArea = bounds.removeFromTop(120);
    waveformDisplay->setBounds(waveformArea);
    
    bounds.removeFromTop(10); // Spacing
    
    // Row 2: Filter, Pitch, Pan+LFO (180px height)  
    auto row2 = bounds.removeFromTop(180);
    auto sectionWidth = row2.getWidth() / 3;
    
    filterSection->setBounds(row2.removeFromLeft(sectionWidth).reduced(5));
    pitchSection->setBounds(row2.removeFromLeft(sectionWidth).reduced(5));
    lfoPanSection->setBounds(row2.reduced(5));
    
    bounds.removeFromTop(10); // Spacing
    
    // Row 3: Chorus, Flanger (remaining height)
    auto effectWidth = bounds.getWidth() / 2;
    chorusSection->setBounds(bounds.removeFromLeft(effectWidth).reduced(5));
    flangerSection->setBounds(bounds.reduced(5));
}

// ================================================================================
// Main Plugin Editor Implementation
// ================================================================================

MyPluginAudioProcessorEditor::MyPluginAudioProcessorEditor(MyPluginAudioProcessor& p)
    : AudioProcessorEditor(p), audioProcessor(p)
{
    setLookAndFeel(&waterfallLAF);
    
    // Create tab components
    mainTab = std::make_unique<MainTabComponent>(audioProcessor);
    advancedTab = std::make_unique<AdvancedTabComponent>(audioProcessor);
    
    // Set up status update callbacks
    mainTab->onStatusUpdate = [this](const juce::String& message) {
        updateStatusText(message);
    };
    advancedTab->onStatusUpdate = [this](const juce::String& message) {
        updateStatusText(message);
    };
    
    // Custom tab buttons with cosmic font
    mainTabButton.setButtonText("𐌌𐌀𐌉𐌍");
    mainTabButton.onClick = [this]() { switchToMainTab(); };
    addAndMakeVisible(mainTabButton);
    
    advancedTabButton.setButtonText("𐌀𐌃𐌅𐌀𐌍𐌂𐌄𐌃");
    advancedTabButton.onClick = [this]() { switchToAdvancedTab(); };
    addAndMakeVisible(advancedTabButton);
    
    // Start with main tab active
    switchToMainTab();
    
    // Status label with cosmic styling
    statusLabel.setText("𐌔𐌵𐌐𐌄𐌓 𐌔𐌀𐌵𐌂𐌄 𐌃𐌄𐌋𐌀𐌙 - 𐌀𐌃𐌅𐌀𐌍𐌂𐌄𐌃 𐌂𐌋𐌀𐌍𐌃𐌵𐌋𐌀𐌓 𐌐𐌓𐌏𐌂𐌄𐌔𐌔𐌉𐌍𐌂 | @arian._.g", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb19cd9));
    addAndMakeVisible(statusLabel);
    
    setSize(900, 730); // Increased for cosmic header
}

MyPluginAudioProcessorEditor::~MyPluginAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MyPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Deep space background
    g.fillAll(juce::Colour(0xff0b0c10));
    
    // Cosmic streak lines
    drawCosmicStreaks(g, bounds);
    
    // Header area
    auto headerArea = bounds.removeFromTop(45);
    
    // Header cosmic gradient
    juce::ColourGradient headerGradient(
        juce::Colour(0x809d4edd), 0, headerArea.getY(),
        juce::Colour(0x40e63946), 0, headerArea.getBottom(), false);
    headerGradient.addColour(0.3, juce::Colour(0x603d5a80));
    headerGradient.addColour(0.7, juce::Colour(0x80f72585));
    g.setGradientFill(headerGradient);
    g.fillRect(headerArea);
    
    // Cosmic glow effect
    juce::ColourGradient glowGradient(
        juce::Colour(0x40f72585), 0, headerArea.getY(),
        juce::Colour(0x00f72585), 0, headerArea.getY() + 20, false);
    g.setGradientFill(glowGradient);
    g.fillRect(headerArea.expanded(0, 10));
    
    // Plugin name in cosmic font
    g.setColour(juce::Colour(0xfff8f8ff)); // Star white
    g.setFont(juce::Font("Arial", 22.0f, juce::Font::bold));
    auto nameArea = headerArea.removeFromLeft(300);
    g.drawText("𐌔𐌵𐌐𐌄𐌓 𐌔𐌀𐌵𐌂𐌄 𐌃𐌄𐌋𐌀𐌙", nameArea.reduced(15, 5), juce::Justification::centredLeft);
    
    // Social media handle
    g.setColour(juce::Colour(0xffb19cd9));
    g.setFont(juce::Font("Arial", 14.0f, juce::Font::italic));
    auto socialArea = headerArea.removeFromRight(120);
    g.drawText("@arian._.g", socialArea.reduced(10, 5), juce::Justification::centredRight);
}

void MyPluginAudioProcessorEditor::drawCosmicStreaks(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::Random random(42); // Fixed seed for consistent pattern
    
    // Draw multiple cosmic streak lines
    for (int i = 0; i < 15; ++i)
    {
        float startX = random.nextFloat() * bounds.getWidth();
        float startY = random.nextFloat() * bounds.getHeight();
        float endX = startX + (random.nextFloat() - 0.5f) * 200;
        float endY = startY + (random.nextFloat() - 0.5f) * 100;
        
        // Choose cosmic colors
        juce::Colour streakColor;
        switch (i % 5)
        {
            case 0: streakColor = juce::Colour(0x409d4edd); break; // Cosmic purple
            case 1: streakColor = juce::Colour(0x30e63946); break; // Cosmic red  
            case 2: streakColor = juce::Colour(0x353d5a80); break; // Cosmic blue
            case 3: streakColor = juce::Colour(0x45f72585); break; // Cosmic pink
            case 4: streakColor = juce::Colour(0x2500d9ff); break; // Cyan
        }
        
        // Draw streak with gradient
        juce::Line<float> streak(startX, startY, endX, endY);
        juce::ColourGradient streakGradient(
            streakColor, startX, startY,
            streakColor.withAlpha(0.0f), endX, endY, false);
        g.setGradientFill(streakGradient);
        
        juce::Path streakPath;
        streakPath.startNewSubPath(startX, startY);
        streakPath.lineTo(endX, endY);
        g.strokePath(streakPath, juce::PathStrokeType(1.5f + random.nextFloat() * 2.0f));
        
        // Add some sparkle points
        if (random.nextFloat() < 0.3f)
        {
            g.setColour(juce::Colour(0xaff8f8ff)); // Bright white
            float sparkleX = startX + (endX - startX) * random.nextFloat();
            float sparkleY = startY + (endY - startY) * random.nextFloat();
            g.fillEllipse(sparkleX - 1, sparkleY - 1, 2, 2);
        }
    }
}

void MyPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Header space (45px)
    auto headerArea = bounds.removeFromTop(45);
    
    // Position custom tab buttons
    // [Main] in the center
    mainTabButton.setBounds(headerArea.getCentreX() - 40, headerArea.getY() + 8, 80, 30);
    
    // [Advanced] in top right corner  
    advancedTabButton.setBounds(headerArea.getWidth() - 120, headerArea.getY() + 8, 110, 30);
    
    // Content area for active tab
    auto contentArea = bounds.removeFromTop(bounds.getHeight() - 35);
    
    if (isMainTabActive && mainTab)
    {
        mainTab->setBounds(contentArea);
    }
    else if (!isMainTabActive && advancedTab)
    {
        advancedTab->setBounds(contentArea);
    }
    
    // Status bar (bottom 35px)
    statusLabel.setBounds(bounds.reduced(15, 8));
}

void MyPluginAudioProcessorEditor::switchToMainTab()
{
    isMainTabActive = true;
    
    if (mainTab)
    {
        addAndMakeVisible(*mainTab);
        mainTab->toBack();
    }
    
    if (advancedTab)
        removeChildComponent(advancedTab.get());
    
    mainTabButton.setToggleState(true, juce::dontSendNotification);
    advancedTabButton.setToggleState(false, juce::dontSendNotification);
    
    resized();
    repaint();
}

void MyPluginAudioProcessorEditor::switchToAdvancedTab()
{
    isMainTabActive = false;
    
    if (advancedTab)
    {
        addAndMakeVisible(*advancedTab);
        advancedTab->toBack();
    }
    
    if (mainTab)
        removeChildComponent(mainTab.get());
    
    mainTabButton.setToggleState(false, juce::dontSendNotification);
    advancedTabButton.setToggleState(true, juce::dontSendNotification);
    
    resized();
    repaint();
}

void MyPluginAudioProcessorEditor::updateStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

// ================================================================================
// Remaining implementations (PresetComboBox, RandomizeButton, GrainVisualizer)
// ================================================================================

// PresetComboBox Implementation (unchanged from original)
PresetComboBox::PresetComboBox(juce::AudioProcessorValueTreeState& vts)
    : valueTreeState(vts)
{
    addAndMakeVisible(presetBox);
    presetBox.addListener(this);
    
    initializePresets();
    
    presetBox.addItem("𐌔𐌵𐌐𐌄𐌓 𐌔𐌀𐌵𐌂𐌄 𐌐𐌓𐌄𐌔𐌄𐌕𐌔 ▼", 1);
    presetBox.addSeparator();
    
    int itemIndex = 2;
    presetBox.addItem("=== 𐌔𐌉𐌂𐌍𐌀𐌕𐌵𐌓𐌄 ===", itemIndex++);
    presetBox.addItem("𐌔𐌵𐌐𐌄𐌓 𐌔𐌀𐌵𐌂𐌄 𐌔𐌐𐌄𐌂𐌉𐌀𐌋", itemIndex++);
    presetBox.addSeparator();
    
    presetBox.addItem("=== 𐌅𐌏𐌂𐌀𐌋 ===", itemIndex++);
    presetBox.addItem("𐌅𐌏𐌂𐌀𐌋 𐌒𐌵𐌀𐌓𐌕𐌄𐌓", itemIndex++);
    presetBox.addItem("𐌅𐌏𐌂𐌀𐌋 𐌔𐌋𐌀𐌐𐌁𐌀𐌂𐌊", itemIndex++);
    presetBox.addSeparator();
    
    presetBox.addItem("=== 𐌂𐋅𐌀𐌓𐌀𐌂𐌕𐌄𐌓 ===", itemIndex++);
    presetBox.addItem("𐌕𐌀𐌐𐌄", itemIndex++);
    presetBox.addItem("𐋅𐌉𐌅𐌉", itemIndex++);
    presetBox.addItem("𐌁𐌁𐌃", itemIndex++);
    presetBox.addItem("𐌃𐌉𐌂𐌉𐌕𐌀𐌋", itemIndex++);
    presetBox.addItem("𐌋𐌏𐌅𐌉", itemIndex++);
    
    presetBox.setSelectedId(1, juce::dontSendNotification);
}

PresetComboBox::~PresetComboBox() = default;

void PresetComboBox::paint(juce::Graphics& g) {}

void PresetComboBox::resized()
{
    presetBox.setBounds(getLocalBounds());
}

void PresetComboBox::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    auto selectedText = comboBoxThatHasChanged->getText();
    
    for (const auto& preset : presets)
    {
        if (preset.name == selectedText)
        {
            loadPreset(preset);
            if (onPresetLoaded)
                onPresetLoaded("Loaded: " + preset.name + " - " + preset.description);
            break;
        }
    }
}

void PresetComboBox::initializePresets()
{
    presets = {
        {"SuperSauce Special", "The signature SuperSauce sound - perfect granular magic",
         {{"delayTime", 320.0f}, {"feedback", 0.4f}, {"mix", 28.0f}, {"eqHigh", 75.0f}, {"eqLow", 25.0f}, 
          {"grainSize", 80.0f}, {"grainDensity", 1.8f}, {"grainSpray", 35.0f}, {"randomization", 25.0f}}},
        
        {"Vocal Quarter", "Perfect for vocal delays - 1/4 note timing with warm tone", 
         {{"delayTime", 400.0f}, {"feedback", 0.2f}, {"mix", 15.0f}, {"eqHigh", 70.0f}, {"eqLow", 30.0f}}},
        
        {"Vocal Slapback", "Classic slapback echo - 80-120ms with brightness",
         {{"delayTime", 100.0f}, {"feedback", 0.1f}, {"mix", 12.0f}, {"eqHigh", 85.0f}, {"eqLow", 15.0f}}},
        
        {"Tape", "Warm analog tape delay with vintage saturation",
         {{"delayTime", 300.0f}, {"feedback", 0.45f}, {"mix", 25.0f}, {"eqHigh", 60.0f}, {"eqLow", 40.0f}}},
        
        {"HiFi", "Clean, pristine digital delay with full bandwidth",
         {{"delayTime", 250.0f}, {"feedback", 0.35f}, {"mix", 20.0f}, {"eqHigh", 95.0f}, {"eqLow", 5.0f}}},
        
        {"BBD", "Bucket brigade delay with classic analog warmth",
         {{"delayTime", 200.0f}, {"feedback", 0.55f}, {"mix", 30.0f}, {"eqHigh", 65.0f}, {"eqLow", 50.0f}}},
        
        {"Digital", "Crystal clear digital delay with precision timing",
         {{"delayTime", 500.0f}, {"feedback", 0.25f}, {"mix", 18.0f}, {"eqHigh", 98.0f}, {"eqLow", 2.0f}}},
        
        {"LoFi", "Degraded delay for vintage lo-fi character",
         {{"delayTime", 350.0f}, {"feedback", 0.7f}, {"mix", 45.0f}, {"eqHigh", 40.0f}, {"eqLow", 80.0f}}}
    };
}

void PresetComboBox::loadPreset(const PresetData& preset)
{
    for (const auto& param : preset.values)
    {
        if (auto* parameter = valueTreeState.getParameter(param.first))
        {
            auto range = parameter->getNormalisableRange();
            auto normalizedValue = range.convertTo0to1(param.second);
            parameter->setValueNotifyingHost(normalizedValue);
        }
    }
}

// RandomizeButton Implementation (unchanged from original)
RandomizeButton::RandomizeButton(juce::AudioProcessorValueTreeState& vts)
    : valueTreeState(vts)
{
    setSize(45, 45);
}

RandomizeButton::~RandomizeButton() = default;

void RandomizeButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    auto bgColor = isHovered ? juce::Colour(0x4064c896) : juce::Colour(0x80000000);
    g.setColour(bgColor);
    g.fillEllipse(bounds);
    
    auto borderColor = isHovered ? juce::Colour(0xff64c896) : juce::Colour(0x8064c896);
    g.setColour(borderColor);
    g.drawEllipse(bounds.reduced(1), 2.0f);
    
    g.setColour(juce::Colours::white);
    auto iconBounds = bounds.reduced(bounds.getWidth() * 0.3f);
    
    g.fillRoundedRectangle(iconBounds, 2.0f);
    
    g.setColour(juce::Colour(0xff1a1a1a));
    auto dotSize = iconBounds.getWidth() * 0.15f;
    auto spacing = iconBounds.getWidth() * 0.25f;
    
    g.fillEllipse(iconBounds.getCentreX() - dotSize/2, iconBounds.getCentreY() - dotSize/2, dotSize, dotSize);
    g.fillEllipse(iconBounds.getX() + spacing, iconBounds.getY() + spacing, dotSize, dotSize);
    g.fillEllipse(iconBounds.getRight() - spacing - dotSize, iconBounds.getY() + spacing, dotSize, dotSize);
    g.fillEllipse(iconBounds.getX() + spacing, iconBounds.getBottom() - spacing - dotSize, dotSize, dotSize);
    g.fillEllipse(iconBounds.getRight() - spacing - dotSize, iconBounds.getBottom() - spacing - dotSize, dotSize, dotSize);
}

void RandomizeButton::resized() {}

void RandomizeButton::mouseDown(const juce::MouseEvent& event)
{
    randomizeParameters();
    
    if (onRandomize)
        onRandomize("𐌔𐌵𐌐𐌄𐌓 𐌔𐌀𐌵𐌂𐌄 parameters cosmically randomized! 🌌 Rolling the interdimensional dice...");
}

void RandomizeButton::mouseEnter(const juce::MouseEvent& event)
{
    isHovered = true;
    repaint();
}

void RandomizeButton::mouseExit(const juce::MouseEvent& event)
{
    isHovered = false;
    repaint();
}

void RandomizeButton::randomizeParameters()
{
    std::vector<juce::String> paramIDs = {
        "delayTime", "feedback", "mix", "grainSize", "grainDensity", 
        "grainSpray", "randomization", "stereoWidth", "eqHigh", "eqLow"
    };
    
    for (const auto& paramID : paramIDs)
    {
        if (auto* parameter = valueTreeState.getParameter(paramID))
        {
            auto randomValue = random.nextFloat();
            parameter->setValueNotifyingHost(randomValue);
        }
    }
    
    if (auto* reverseParam = valueTreeState.getParameter("reverseGrains"))
    {
        reverseParam->setValueNotifyingHost(random.nextBool() ? 1.0f : 0.0f);
    }
}

// GrainVisualizer Implementation (unchanged from original)
GrainVisualizer::GrainVisualizer()
{
    startTimerHz(30);
    visualGrains.resize(50);
}

GrainVisualizer::~GrainVisualizer()
{
    stopTimer();
}

void GrainVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colour(0x80000000));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    
    g.setColour(juce::Colour(0x4064c896));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 1.0f);
    
    for (const auto& grain : visualGrains)
    {
        if (grain.opacity > 0.0f)
        {
            g.setColour(grain.color.withAlpha(grain.opacity));
            
            auto grainBounds = juce::Rectangle<float>(
                grain.x - grain.size/2, 
                grain.y - grain.size/2, 
                grain.size, 
                grain.size
            );
            
            if (grain.isReverse)
            {
                juce::Path diamond;
                diamond.addQuadrilateral(
                    grain.x, grain.y - grain.size/2,
                    grain.x + grain.size/2, grain.y,
                    grain.x, grain.y + grain.size/2,
                    grain.x - grain.size/2, grain.y
                );
                g.fillPath(diamond);
            }
            else
            {
                g.fillEllipse(grainBounds);
            }
            
            g.setColour(grain.color.withAlpha(grain.opacity * 0.3f));
            g.fillEllipse(grainBounds.expanded(2.0f));
        }
    }
    
    g.setColour(juce::Colour(0xffa0c0a0));
    g.setFont(12.0f);
    g.drawText("Grain Cloud", bounds.removeFromTop(20), juce::Justification::centred);
}

void GrainVisualizer::timerCallback()
{
    updateGrains();
    repaint();
}

void GrainVisualizer::updateGrainActivity(float density, float size, bool reverse)
{
    currentDensity = density;
    currentSize = size;
    currentReverse = reverse;
}

void GrainVisualizer::updateGrains()
{
    auto bounds = getLocalBounds().reduced(20);
    
    for (auto& grain : visualGrains)
    {
        if (grain.opacity > 0.0f)
        {
            grain.age += 0.033f;
            grain.opacity = juce::jmax(0.0f, 1.0f - grain.age * 2.0f);
            
            grain.x += (random.nextFloat() - 0.5f) * 0.5f;
            grain.y += (random.nextFloat() - 0.5f) * 0.5f;
            
            grain.x = juce::jlimit((float)bounds.getX(), (float)bounds.getRight(), grain.x);
            grain.y = juce::jlimit((float)bounds.getY(), (float)bounds.getBottom(), grain.y);
        }
    }
    
    float spawnProbability = currentDensity * 0.1f;
    
    if (random.nextFloat() < spawnProbability)
    {
        for (auto& grain : visualGrains)
        {
            if (grain.opacity <= 0.0f)
            {
                grain.x = bounds.getX() + random.nextFloat() * bounds.getWidth();
                grain.y = bounds.getY() + random.nextFloat() * bounds.getHeight();
                grain.size = juce::jmap(currentSize, 5.0f, 200.0f, 2.0f, 8.0f);
                grain.opacity = 0.8f + random.nextFloat() * 0.2f;
                grain.age = 0.0f;
                grain.isReverse = currentReverse;
                
                if (grain.isReverse)
                {
                    grain.color = juce::Colour(0xff4ecdc4);
                }
                else
                {
                    grain.color = juce::Colour(0xff64c896);
                }
                
                break;
            }
        }
    }
}