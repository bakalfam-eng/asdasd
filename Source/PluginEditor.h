#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

class SproutLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SproutLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosition, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = 1024;
    static constexpr int spectrumBands = 72;
    static constexpr int pitchBands = 48;
    static constexpr int historyColumns = 128;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void analyseFrame();
    void drawCreature(juce::Graphics&, juce::Rectangle<float>);
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float>);
    void drawPitchTrails(juce::Graphics&, juce::Rectangle<float>);
    void drawPanel(juce::Graphics&, juce::Rectangle<float>, const juce::String&);
    static juce::Colour pitchColour(int pitch);

    PluginProcessor& processor;
    SproutLookAndFeel lookAndFeel;

    juce::Slider sensitivitySlider;
    juce::Slider motionSlider;
    juce::Slider glowSlider;
    juce::Label sensitivityLabel;
    juce::Label motionLabel;
    juce::Label glowLabel;

    std::unique_ptr<Attachment> sensitivityAttachment;
    std::unique_ptr<Attachment> motionAttachment;
    std::unique_ptr<Attachment> glowAttachment;

    std::atomic<float>* sensitivityParameter = nullptr;
    std::atomic<float>* motionParameter = nullptr;
    std::atomic<float>* glowParameter = nullptr;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window {
        static_cast<size_t>(fftSize), juce::dsp::WindowingFunction<float>::hann, false
    };

    std::array<float, fftSize> sampleRing {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, 32768> readBuffer {};
    std::array<float, spectrumBands> spectrum {};
    std::array<float, spectrumBands> spectrumTarget {};
    std::array<float, pitchBands> pitches {};
    std::array<float, pitchBands> pitchTarget {};
    std::array<std::array<float, pitchBands>, historyColumns> history {};

    int sampleWritePosition = 0;
    int samplesCollected = 0;
    int samplesSinceFrame = 0;
    int historyWritePosition = 0;
    int silentTicks = 0;

    double currentSampleRate = 44100.0;
    double previousTickTime = 0.0;
    float time = 0.0f;
    float scrollPhase = 0.0f;
    float level = 0.0f;
    float previousLevel = 0.0f;
    float beatPulse = 0.0f;
    float glow = 0.65f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
