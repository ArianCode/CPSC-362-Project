#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom Look and Feel for Waterfall Theme
//==============================================================================
class WaterfallLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WaterfallLookAndFeel();
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override;
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                     int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                     juce::ComboBox& box) override;
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, 
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

private:
    juce::Colour waterfallPrimary   = juce::Colour(0xff64c896);
    juce::Colour waterfallSecondary = juce::Colour(0xff4a90e2);
    juce::Colour darkGreen          = juce::Colour(0xff1a2f1a);
    juce::Colour backgroundDark     = juce::Colour(0xff0d1a0d);
};


//==============================================================================
// Custom Knob Component
//==============================================================================
class CustomKnob : public juce::Component
{
public:
    CustomKnob(juce::AudioProcessorValueTreeState& vts, const juce::String& paramID, 
               const juce::String& labelText, const juce::String& tooltipText);
    ~CustomKnob() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    
    std::function<void(const juce::String&)> onHover;
    
private:
    juce::Slider slider;
    juce::Label label;
    juce::String tooltip;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomKnob)
};

//==============================================================================
// Preset ComboBox Component
//==============================================================================
class PresetComboBox : public juce::Component, public juce::ComboBox::Listener
{
public:
    PresetComboBox(juce::AudioProcessorValueTreeState& vts);
    ~PresetComboBox() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    
    std::function<void(const juce::String&)> onPresetLoaded;
    
private:
    juce::ComboBox presetBox;
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    struct PresetData {
        juce::String name;
        juce::String description;
        std::map<juce::String, float> values;
    };
    
    std::vector<PresetData> presets;
    void initializePresets();
    void loadPreset(const PresetData& preset);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetComboBox)
};

//==============================================================================
// Randomize Button Component
//==============================================================================
class RandomizeButton : public juce::Component
{
public:
    RandomizeButton(juce::AudioProcessorValueTreeState& vts);
    ~RandomizeButton() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    
    std::function<void(const juce::String&)> onRandomize;
    
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    bool isHovered = false;
    juce::Random random;
    
    void randomizeParameters();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomizeButton)
};

//==============================================================================
// Grain Visualizer Component
//==============================================================================
class GrainVisualizer : public juce::Component, public juce::Timer
{
public:
    GrainVisualizer();
    ~GrainVisualizer() override;
    
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    
    void updateGrainActivity(float density, float size, bool reverse);
    
private:
    struct VisualGrain {
        float x, y;
        float size;
        float opacity;
        float age;
        bool isReverse;
        juce::Colour color;
    };
    
    std::vector<VisualGrain> visualGrains;
    juce::Random random;
    float currentDensity = 1.0f;
    float currentSize = 50.0f;
    bool currentReverse = false;
    
    void updateGrains();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainVisualizer)
};

//==============================================================================
// Real-time Waveform Display
//==============================================================================
class WaveformDisplay : public juce::Component, public juce::Timer
{
public:
    WaveformDisplay(MyPluginAudioProcessor& processor);
    ~WaveformDisplay() override;
    
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    
private:
    MyPluginAudioProcessor& audioProcessor;
    juce::Path waveformPath;
    std::array<float, 512> displayBuffer{};
    bool needsRepaint = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

//==============================================================================
// Filter Section Component
//==============================================================================
class FilterSection : public juce::Component
{
public:
    FilterSection(juce::AudioProcessorValueTreeState& vts);
    ~FilterSection() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void(const juce::String&)> onHover;
    
private:
    std::unique_ptr<CustomKnob> cutoffKnob;
    std::unique_ptr<CustomKnob> resonanceKnob;
    
    juce::ComboBox filterTypeBox;
    juce::Label filterTypeLabel;
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterSection)
};

//==============================================================================
// Pitch Section Component
//==============================================================================
class PitchSection : public juce::Component
{
public:
    PitchSection(juce::AudioProcessorValueTreeState& vts);
    ~PitchSection() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void(const juce::String&)> onHover;
    
private:
    std::unique_ptr<CustomKnob> semitonesKnob;
    std::unique_ptr<CustomKnob> octavesKnob;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchSection)
};

//==============================================================================
// LFO & Pan Section Component
//==============================================================================
class LfoPanSection : public juce::Component
{
public:
    LfoPanSection(juce::AudioProcessorValueTreeState& vts);
    ~LfoPanSection() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void(const juce::String&)> onHover;
    
private:
    // LFO controls
    std::unique_ptr<CustomKnob> lfoRateKnob;
    std::unique_ptr<CustomKnob> lfoDepthKnob;
    
    juce::ComboBox lfoTargetBox;
    juce::ComboBox lfoWaveformBox;
    juce::ToggleButton lfoBipolarButton;
    juce::ToggleButton lfoSyncButton;
    juce::ComboBox lfoSyncDivisionBox;
    
    // Pan control
    std::unique_ptr<CustomKnob> panKnob;
    
    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoTargetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoBipolarAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSyncDivisionAttachment;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanSection)
};

//==============================================================================
// Chorus Section Component  
//==============================================================================
class ChorusSection : public juce::Component
{
public:
    ChorusSection(juce::AudioProcessorValueTreeState& vts);
    ~ChorusSection() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void(const juce::String&)> onHover;
    
private:
    std::unique_ptr<CustomKnob> rateKnob;
    std::unique_ptr<CustomKnob> depthKnob;
    std::unique_ptr<CustomKnob> mixKnob;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusSection)
};

//==============================================================================
// Flanger Section Component
//==============================================================================
class FlangerSection : public juce::Component
{
public:
    FlangerSection(juce::AudioProcessorValueTreeState& vts);
    ~FlangerSection() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void(const juce::String&)> onHover;
    
private:
    std::unique_ptr<CustomKnob> delayKnob;
    std::unique_ptr<CustomKnob> feedbackKnob;
    std::unique_ptr<CustomKnob> depthKnob;
    std::unique_ptr<CustomKnob> rateKnob;
    std::unique_ptr<CustomKnob> mixKnob;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlangerSection)
};

//==============================================================================
// Main Tab Component
//==============================================================================
class MainTabComponent : public juce::Component, public juce::Timer
{
public:
    MainTabComponent(MyPluginAudioProcessor& processor);
    ~MainTabComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    std::function<void(const juce::String&)> onStatusUpdate;
    
private:
    MyPluginAudioProcessor& audioProcessor;
    
    // UI Components
    std::unique_ptr<PresetComboBox> presetSelector;
    std::unique_ptr<RandomizeButton> randomizeBtn;
    
    // Control sections
    std::vector<std::unique_ptr<CustomKnob>> delayKnobs;
    std::vector<std::unique_ptr<CustomKnob>> eqKnobs;
    std::vector<std::unique_ptr<CustomKnob>> granularKnobs;
    std::vector<std::unique_ptr<CustomKnob>> creativeKnobs;
    
    // Visualization
    std::unique_ptr<GrainVisualizer> grainViz;
    
    // Mix slider
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    
    void setupControls();
    void setupSections();
    void drawWaterfallBackground(juce::Graphics& g);
    void drawCosmicStreaks(juce::Graphics& g, juce::Rectangle<int> bounds);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainTabComponent)
};

//==============================================================================
// Advanced Tab Component
//==============================================================================
class AdvancedTabComponent : public juce::Component
{
public:
    AdvancedTabComponent(MyPluginAudioProcessor& processor);
    ~AdvancedTabComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void(const juce::String&)> onStatusUpdate;
    
private:
    MyPluginAudioProcessor& audioProcessor;
    
    // Sections
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<FilterSection> filterSection;
    std::unique_ptr<PitchSection> pitchSection;
    std::unique_ptr<LfoPanSection> lfoPanSection;
    std::unique_ptr<ChorusSection> chorusSection;
    std::unique_ptr<FlangerSection> flangerSection;
    
    void drawWaterfallBackground(juce::Graphics& g);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedTabComponent)
};

//==============================================================================
// Main Plugin Editor
//==============================================================================
class MyPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    MyPluginAudioProcessorEditor (MyPluginAudioProcessor& p);
    ~MyPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MyPluginAudioProcessor& audioProcessor;
    
    // Look and feel
    WaterfallLookAndFeel waterfallLAF;
    
    // Custom tab system (no standard tabbed component)
    juce::TextButton mainTabButton;
    juce::TextButton advancedTabButton;
    bool isMainTabActive = true;
    
    // Tab components
    std::unique_ptr<MainTabComponent> mainTab;
    std::unique_ptr<AdvancedTabComponent> advancedTab;
    
    // Status display
    juce::Label statusLabel;
    
    void updateStatusText(const juce::String& text);
    void drawCosmicStreaks(juce::Graphics& g, juce::Rectangle<int> bounds);
    void switchToMainTab();
    void switchToAdvancedTab();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyPluginAudioProcessorEditor)
};