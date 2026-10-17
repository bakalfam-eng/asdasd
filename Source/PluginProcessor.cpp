#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <memory>

juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "sensitivity", 1 },
        "Sensitivity",
        juce::NormalisableRange<float> { 0.5f, 3.0f, 0.01f },
        1.25f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "motion", 1 },
        "Motion",
        juce::NormalisableRange<float> { 0.5f, 2.0f, 0.01f },
        1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "glow", 1 },
        "Glow",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.65f));

    return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "GrooveSproutState", createParameterLayout())
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    analysisSampleRate.store(sampleRate > 0.0 ? sampleRate : 44100.0,
                             std::memory_order_relaxed);
    analysisLevel.store(0.0f, std::memory_order_relaxed);

    // The queue has one audio-thread writer and one message-thread reader.
    // Do not reset its indices while an editor may be reading.
}

void PluginProcessor::releaseResources()
{
    analysisLevel.store(0.0f, std::memory_order_relaxed);
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Deliberately do not modify audio or MIDI. All controls are visual only.
    if (!analysisEnabled.load(std::memory_order_relaxed)
        || buffer.getNumChannels() < 2
        || buffer.getNumSamples() == 0)
        return;

    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getReadPointer(1);
    const int sampleCount = buffer.getNumSamples();

    double sumLeft = 0.0;
    double sumRight = 0.0;

    for (int i = 0; i < sampleCount; ++i)
    {
        const float l = std::isfinite(left[i]) ? juce::jlimit(-16.0f, 16.0f, left[i]) : 0.0f;
        const float r = std::isfinite(right[i]) ? juce::jlimit(-16.0f, 16.0f, right[i]) : 0.0f;
        sumLeft += static_cast<double>(l) * static_cast<double>(l);
        sumRight += static_cast<double>(r) * static_cast<double>(r);
    }

    analysisLevel.store(
        static_cast<float>(std::sqrt((sumLeft + sumRight)
                                    / (2.0 * static_cast<double>(sampleCount)))),
        std::memory_order_relaxed);

    // Use the louder channel instead of summing: anti-phase stereo still
    // produces a useful display. This choice affects analysis only.
    const float* source = sumLeft >= sumRight ? left : right;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    analysisFifo.prepareToWrite(sampleCount, start1, size1, start2, size2);

    const auto copySection = [this, source](int destination, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            const float value = source[offset + i];
            analysisQueue[static_cast<size_t>(destination + i)] =
                std::isfinite(value) ? juce::jlimit(-16.0f, 16.0f, value) : 0.0f;
        }
    };

    copySection(start1, size1, 0);
    copySection(start2, size2, size1);
    analysisFifo.finishedWrite(size1 + size2);
}

int PluginProcessor::readAnalysisSamples(float* destination, int maximumSamples)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    analysisFifo.prepareToRead(maximumSamples, start1, size1, start2, size2);

    std::copy_n(analysisQueue.data() + start1, size1, destination);
    std::copy_n(analysisQueue.data() + start2, size2, destination + size1);
    analysisFifo.finishedRead(size1 + size2);
    return size1 + size2;
}

void PluginProcessor::setAnalysisEnabled(bool enabled) noexcept
{
    analysisEnabled.store(enabled, std::memory_order_relaxed);
}

float PluginProcessor::getAnalysisLevel() const noexcept
{
    return analysisLevel.load(std::memory_order_relaxed);
}

double PluginProcessor::getAnalysisSampleRate() const noexcept
{
    return analysisSampleRate.load(std::memory_order_relaxed);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType().toString()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
