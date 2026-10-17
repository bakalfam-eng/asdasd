#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    const juce::Colour background { 0xff111827 };
    const juce::Colour panelColour { 0xff192338 };
    const juce::Colour mint { 0xff8ef0c5 };
    const juce::Colour pink { 0xffff91bd };
    const juce::Colour textColour { 0xffeef4ff };
    const juce::Colour muted { 0xff8e9fba };

    void setFont(juce::Graphics& g, float height, bool bold = false)
    {
        g.setFont(juce::Font(juce::FontOptions(
            height, bold ? juce::Font::bold : juce::Font::plain)));
    }

    void drawStar(juce::Graphics& g, float x, float y, float radius,
                  juce::Colour colour)
    {
        juce::Path star;
        star.startNewSubPath(x, y - radius);
        star.lineTo(x + radius * 0.28f, y - radius * 0.28f);
        star.lineTo(x + radius, y);
        star.lineTo(x + radius * 0.28f, y + radius * 0.28f);
        star.lineTo(x, y + radius);
        star.lineTo(x - radius * 0.28f, y + radius * 0.28f);
        star.lineTo(x - radius, y);
        star.lineTo(x - radius * 0.28f, y - radius * 0.28f);
        star.closeSubPath();
        g.setColour(colour);
        g.fillPath(star);
    }
}

SproutLookAndFeel::SproutLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, mint);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2d3b52));
    setColour(juce::Slider::thumbColourId, textColour);
    setColour(juce::Label::textColourId, muted);
    setColour(juce::TooltipWindow::backgroundColourId, panelColour);
    setColour(juce::TooltipWindow::textColourId, textColour);
}

void SproutLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(
        static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(width), static_cast<float>(height)).reduced(7.0f);

    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle
                      + sliderPosition * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(track, juce::PathStrokeType(3.0f));

    juce::Path active;
    active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                        rotaryStartAngle, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(active, juce::PathStrokeType(
        3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xff25344b));
    g.fillEllipse(centre.x - radius + 5.0f, centre.y - radius + 5.0f,
                  (radius - 5.0f) * 2.0f, (radius - 5.0f) * 2.0f);

    const float pointerRadius = radius - 9.0f;
    const float px = centre.x + std::sin(angle) * pointerRadius;
    const float py = centre.y - std::cos(angle) * pointerRadius;
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.drawLine(centre.x, centre.y, px, py, 2.3f);
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    sensitivityParameter = processor.apvts.getRawParameterValue("sensitivity");
    motionParameter = processor.apvts.getRawParameterValue("motion");
    glowParameter = processor.apvts.getRawParameterValue("glow");

    const auto configure = [this](juce::Slider& slider, juce::Label& label,
                                   const juce::String& name,
                                   const juce::String& tooltip)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(pi * 1.25f, pi * 2.75f, true);
        slider.setName(name);
        slider.setTooltip(tooltip);
        slider.setPopupDisplayEnabled(true, true, this);
        addAndMakeVisible(slider);

        label.setText(name, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(11.0f)));
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    };

    configure(sensitivitySlider, sensitivityLabel, "Sensitivity",
              "Visual sensitivity only. Your audio stays unchanged.");
    configure(motionSlider, motionLabel, "Motion",
              "Dance speed and pitch-trail scrolling speed.");
    configure(glowSlider, glowLabel, "Glow",
              "Sparkles and luminous trails.");

    motionSlider.setColour(juce::Slider::rotarySliderFillColourId, pink);
    glowSlider.setColour(juce::Slider::rotarySliderFillColourId,
                         juce::Colour(0xffb5a3ff));

    sensitivityAttachment = std::make_unique<Attachment>(
        processor.apvts, "sensitivity", sensitivitySlider);
    motionAttachment = std::make_unique<Attachment>(
        processor.apvts, "motion", motionSlider);
    glowAttachment = std::make_unique<Attachment>(
        processor.apvts, "glow", glowSlider);

    // Discard samples left by a previously closed editor.
    processor.readAnalysisSamples(readBuffer.data(), static_cast<int>(readBuffer.size()));
    currentSampleRate = processor.getAnalysisSampleRate();
    previousTickTime = juce::Time::getMillisecondCounterHiRes();
    processor.setAnalysisEnabled(true);

    setSize(640, 400);
    startTimerHz(60);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    processor.setAnalysisEnabled(false);
    setLookAndFeel(nullptr);
}

void PluginEditor::resized()
{
    sensitivitySlider.setBounds(325, 316, 70, 61);
    motionSlider.setBounds(425, 316, 70, 61);
    glowSlider.setBounds(525, 316, 70, 61);

    sensitivityLabel.setBounds(310, 377, 100, 18);
    motionLabel.setBounds(410, 377, 100, 18);
    glowLabel.setBounds(510, 377, 100, 18);
}

void PluginEditor::analyseFrame()
{
    for (int i = 0; i < fftSize; ++i)
        fftData[static_cast<size_t>(i)] =
            sampleRing[static_cast<size_t>((sampleWritePosition + i) % fftSize)];

    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
    fft.performFrequencyOnlyForwardTransform(fftData.data(), true);

    const float sensitivity = sensitivityParameter->load(std::memory_order_relaxed);
    const float magnitudeScale = 4.0f * sensitivity / static_cast<float>(fftSize);
    const double binHz = currentSampleRate / static_cast<double>(fftSize);
    const double topFrequency = juce::jmin(20000.0, currentSampleRate * 0.49);

    for (int band = 0; band < spectrumBands; ++band)
    {
        const double startFraction =
            static_cast<double>(band) / static_cast<double>(spectrumBands);
        const double endFraction =
            static_cast<double>(band + 1) / static_cast<double>(spectrumBands);
        const double lowHz = 30.0 * std::pow(topFrequency / 30.0, startFraction);
        const double highHz = 30.0 * std::pow(topFrequency / 30.0, endFraction);

        const int firstBin = juce::jlimit(
            1, fftSize / 2 - 1, static_cast<int>(std::floor(lowHz / binHz)));
        const int lastBin = juce::jlimit(
            firstBin, fftSize / 2 - 1, static_cast<int>(std::ceil(highHz / binHz)));

        float magnitude = 0.0f;
        for (int bin = firstBin; bin <= lastBin; ++bin)
            magnitude = juce::jmax(magnitude, fftData[static_cast<size_t>(bin)]);

        const float decibels =
            juce::Decibels::gainToDecibels(magnitude * magnitudeScale, -90.0f);
        spectrumTarget[static_cast<size_t>(band)] =
            juce::jlimit(0.0f, 1.0f, (decibels + 78.0f) / 78.0f);
    }

    pitchTarget.fill(0.0f);

    // Spectral peaks form a playful polyphonic pitch-activity view.
    // Harmonics are intentionally included; this is not a note transcription.
    const int lastPitchBin = juce::jlimit(
        2, fftSize / 2 - 2, static_cast<int>(2200.0 / binHz));

    for (int bin = 2; bin <= lastPitchBin; ++bin)
    {
        const float a = fftData[static_cast<size_t>(bin - 1)];
        const float b = fftData[static_cast<size_t>(bin)];
        const float c = fftData[static_cast<size_t>(bin + 1)];

        if (b <= a || b < c || b * magnitudeScale < 0.001f)
            continue;

        // Log-magnitude parabolic interpolation improves the pitch position.
        const float la = std::log(juce::jmax(a, 1.0e-9f));
        const float lb = std::log(juce::jmax(b, 1.0e-9f));
        const float lc = std::log(juce::jmax(c, 1.0e-9f));
        const float denominator = la - 2.0f * lb + lc;
        const float offset = std::abs(denominator) > 1.0e-8f
                           ? juce::jlimit(-0.5f, 0.5f, 0.5f * (la - lc) / denominator)
                           : 0.0f;

        const double frequency = (static_cast<double>(bin)
                                  + static_cast<double>(offset)) * binHz;
        const float pitchPosition = static_cast<float>(
            69.0 + 12.0 * std::log2(frequency / 440.0) - 36.0);

        const int nearest = static_cast<int>(std::lround(pitchPosition));
        const float strength = std::pow(
            juce::jlimit(0.0f, 1.0f, b * magnitudeScale * 11.0f), 0.65f);

        for (int neighbour = -1; neighbour <= 1; ++neighbour)
        {
            const int index = nearest + neighbour;
            if (index < 0 || index >= pitchBands)
                continue;

            const float distance = std::abs(pitchPosition - static_cast<float>(index));
            const float weight = juce::jmax(0.0f, 1.0f - distance * 0.8f);
            auto& target = pitchTarget[static_cast<size_t>(index)];
            target = juce::jmax(target, strength * weight);
        }
    }
}

void PluginEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = juce::jlimit(
        0.001f, 0.1f, static_cast<float>((now - previousTickTime) * 0.001));
    previousTickTime = now;

    const double newSampleRate = processor.getAnalysisSampleRate();
    if (newSampleRate != currentSampleRate)
    {
        currentSampleRate = newSampleRate;
        sampleRing.fill(0.0f);
        sampleWritePosition = 0;
        samplesCollected = 0;
        samplesSinceFrame = 0;
    }

    const int count = processor.readAnalysisSamples(
        readBuffer.data(), static_cast<int>(readBuffer.size()));

    silentTicks = count > 0 ? 0 : juce::jmin(silentTicks + 1, 1000);

    for (int i = 0; i < count; ++i)
    {
        sampleRing[static_cast<size_t>(sampleWritePosition)] =
            readBuffer[static_cast<size_t>(i)];
        sampleWritePosition = (sampleWritePosition + 1) % fftSize;
        samplesCollected = juce::jmin(samplesCollected + 1, fftSize);
        ++samplesSinceFrame;

        if (samplesSinceFrame >= hopSize && samplesCollected == fftSize)
        {
            samplesSinceFrame = 0;
            analyseFrame();
        }
    }

    const float fastSmoothing = 1.0f - std::exp(-dt * 24.0f);
    const float slowSmoothing = 1.0f - std::exp(-dt * 7.0f);
    const float targetDecay = std::exp(-dt * (silentTicks > 8 ? 9.0f : 1.8f));

    for (int i = 0; i < spectrumBands; ++i)
    {
        auto& value = spectrum[static_cast<size_t>(i)];
        auto& target = spectrumTarget[static_cast<size_t>(i)];
        value += (target - value) * (target > value ? fastSmoothing : slowSmoothing);
        target *= targetDecay;
    }

    for (int i = 0; i < pitchBands; ++i)
    {
        auto& value = pitches[static_cast<size_t>(i)];
        auto& target = pitchTarget[static_cast<size_t>(i)];
        value += (target - value) * fastSmoothing;
        target *= targetDecay;
    }

    const float sensitivity = sensitivityParameter->load(std::memory_order_relaxed);
    const float motion = motionParameter->load(std::memory_order_relaxed);
    glow = glowParameter->load(std::memory_order_relaxed);

    const float desiredLevel = silentTicks > 8 ? 0.0f
        : juce::jlimit(0.0f, 1.0f, processor.getAnalysisLevel() * sensitivity * 4.0f);

    level += (desiredLevel - level)
           * (desiredLevel > level ? fastSmoothing : slowSmoothing);

    beatPulse *= std::exp(-dt * 5.5f);
    if (desiredLevel > previousLevel + 0.035f)
        beatPulse = juce::jmax(beatPulse,
                              juce::jlimit(0.0f, 1.0f,
                                           (desiredLevel - previousLevel) * 4.0f));
    previousLevel = desiredLevel;

    time += dt * motion;
    if (time > 10000.0f)
        time -= 10000.0f;

    scrollPhase += dt * 16.0f * motion;
    while (scrollPhase >= 1.0f)
    {
        history[static_cast<size_t>(historyWritePosition)] = pitches;
        historyWritePosition = (historyWritePosition + 1) % historyColumns;
        scrollPhase -= 1.0f;
    }

    repaint();
}

juce::Colour PluginEditor::pitchColour(int pitch)
{
    const float hue = std::fmod(
        0.52f + static_cast<float>(pitch) / static_cast<float>(pitchBands) * 0.72f,
        1.0f);
    return juce::Colour::fromHSV(hue, 0.53f, 1.0f, 1.0f);
}

void PluginEditor::drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                             const juce::String& title)
{
    g.setColour(panelColour);
    g.fillRoundedRectangle(bounds, 15.0f);
    g.setColour(juce::Colours::white.withAlpha(0.055f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 15.0f, 1.0f);
    setFont(g, 10.0f, true);
    g.setColour(muted);
    g.drawText(title, bounds.toNearestInt().withTrimmedLeft(13).withTrimmedTop(8)
                            .withHeight(15),
               juce::Justification::centredLeft);
}

void PluginEditor::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    drawPanel(g, bounds, "FREQUENCY GARDEN");
    auto graph = bounds.reduced(13.0f, 0.0f).withTrimmedTop(30.0f)
                       .withTrimmedBottom(20.0f);

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = graph.getY() + graph.getHeight() * static_cast<float>(i) / 4.0f;
        g.drawHorizontalLine(static_cast<int>(y), graph.getX(), graph.getRight());
    }

    juce::Path line;
    for (int i = 0; i < spectrumBands; ++i)
    {
        const float x = graph.getX() + graph.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(spectrumBands - 1);
        const float y = graph.getBottom()
                      - spectrum[static_cast<size_t>(i)] * graph.getHeight();
        if (i == 0)
            line.startNewSubPath(x, y);
        else
            line.lineTo(x, y);
    }

    auto fill = line;
    fill.lineTo(graph.getRight(), graph.getBottom());
    fill.lineTo(graph.getX(), graph.getBottom());
    fill.closeSubPath();

    g.setGradientFill(juce::ColourGradient(
        mint.withAlpha(0.34f), graph.getX(), graph.getY(),
        mint.withAlpha(0.015f), graph.getX(), graph.getBottom(), false));
    g.fillPath(fill);

    if (glow > 0.01f)
    {
        g.setColour(mint.withAlpha(glow * 0.12f));
        g.strokePath(line, juce::PathStrokeType(7.0f));
    }
    g.setColour(mint);
    g.strokePath(line, juce::PathStrokeType(
        1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    setFont(g, 9.0f);
    g.setColour(muted);
    auto axis = bounds.toNearestInt().reduced(13, 0)
                      .withY(static_cast<int>(bounds.getBottom()) - 17).withHeight(12);
    g.drawText("30 Hz", axis, juce::Justification::centredLeft);
    g.drawText("1 kHz", axis, juce::Justification::centred);
    const auto upperText = currentSampleRate >= 40817.0 ? juce::String("20 kHz")
        : juce::String(currentSampleRate * 0.49 / 1000.0, 1) + " kHz";
    g.drawText(upperText, axis, juce::Justification::centredRight);
}

void PluginEditor::drawPitchTrails(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    drawPanel(g, bounds, "PITCH RIBBONS");
    setFont(g, 9.0f);
    g.setColour(muted);
    g.drawText("harmonics welcome",
               bounds.toNearestInt().reduced(13, 0).withTrimmedTop(9).withHeight(14),
               juce::Justification::centredRight);

    const auto plot = bounds.reduced(13.0f, 0.0f).withTrimmedTop(30.0f)
                            .withTrimmedBottom(20.0f);
    const float columnWidth = plot.getWidth() / static_cast<float>(historyColumns);
    const float rowHeight = plot.getHeight() / static_cast<float>(pitchBands);

    g.setColour(juce::Colours::white.withAlpha(0.045f));
    for (int octave = 1; octave < 4; ++octave)
    {
        const float y = plot.getBottom()
                      - plot.getHeight() * static_cast<float>(octave) / 4.0f;
        g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
    }

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(plot.toNearestInt());

        for (int column = 0; column < historyColumns; ++column)
        {
            const auto& frame = history[static_cast<size_t>(
                (historyWritePosition + column) % historyColumns)];
            const float ageFade = 0.15f + 0.85f * std::sqrt(
                static_cast<float>(column) / static_cast<float>(historyColumns - 1));
            const float x = plot.getX() + static_cast<float>(column) * columnWidth;

            for (int pitch = 0; pitch < pitchBands; ++pitch)
            {
                const float strength = frame[static_cast<size_t>(pitch)];
                if (strength < 0.035f)
                    continue;

                const float y = plot.getBottom()
                              - static_cast<float>(pitch + 1) * rowHeight;
                const auto colour = pitchColour(pitch);
                const float alpha = juce::jlimit(0.0f, 1.0f, strength * ageFade);

                if (glow > 0.05f && strength > 0.2f)
                {
                    g.setColour(colour.withAlpha(alpha * glow * 0.15f));
                    g.fillRoundedRectangle(x - 1.0f, y - 2.0f,
                                           columnWidth + 2.5f, rowHeight + 4.0f, 2.0f);
                }

                g.setColour(colour.withAlpha(alpha));
                g.fillRoundedRectangle(x, y, columnWidth + 0.4f,
                                       juce::jmax(1.0f, rowHeight - 0.15f), 0.7f);
            }
        }
    }

    g.setColour(pink.withAlpha(0.45f));
    g.drawVerticalLine(static_cast<int>(plot.getRight()), plot.getY(), plot.getBottom());

    setFont(g, 9.0f);
    g.setColour(muted);
    const auto footer = bounds.toNearestInt().reduced(13, 0)
                              .withY(static_cast<int>(bounds.getBottom()) - 17)
                              .withHeight(12);
    g.drawText("PAST", footer, juce::Justification::centredLeft);
    g.drawText("low to high / bottom to top", footer, juce::Justification::centred);
    g.drawText("NOW", footer, juce::Justification::centredRight);
}

void PluginEditor::drawCreature(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    drawPanel(g, bounds, "YOUR POCKET GROOVE");

    const float centreX = bounds.getCentreX();
    const float floorY = bounds.getBottom() - 39.0f;
    const float dance = std::sin(time * 7.0f);
    const float sway = std::sin(time * 3.5f) * (2.0f + level * 9.0f);
    const float bounce = std::abs(dance) * level * 18.0f + beatPulse * 12.0f;
    const float bodyX = centreX + sway;
    const float bodyY = floorY - 48.0f - bounce;
    const float squash = 1.0f + level * 0.065f * std::cos(time * 14.0f);
    const float bodyWidth = 87.0f * squash;
    const float bodyHeight = 83.0f / squash;

    // Floating confetti responds to musical energy without flashing.
    for (int i = 0; i < 9; ++i)
    {
        const float index = static_cast<float>(i);
        const float phase = time * (0.22f + index * 0.013f) + index * 1.71f;
        const float x = centreX + std::sin(phase) * (57.0f + index * 2.2f);
        const float y = bounds.getY() + 47.0f
                      + (0.5f + 0.5f * std::cos(phase * 1.3f)) * 119.0f;
        const float alpha = (0.10f + level * 0.50f) * glow;
        drawStar(g, x, y, 2.0f + level * 2.5f,
                 (i % 2 == 0 ? mint : pink).withAlpha(alpha));
    }

    g.setColour(mint.withAlpha(0.045f + glow * level * 0.09f));
    g.fillEllipse(centreX - 69.0f, floorY - 14.0f, 138.0f, 30.0f);
    g.setColour(juce::Colour(0xff0c1422).withAlpha(0.55f));
    const float shadowWidth = 72.0f - bounce * 0.7f;
    g.fillEllipse(centreX - shadowWidth * 0.5f, floorY - 3.0f,
                  shadowWidth, 12.0f);

    const auto stroke = juce::PathStrokeType(
        8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    g.setColour(mint.darker(0.20f));
    juce::Path arms;
    arms.startNewSubPath(bodyX - 35.0f, bodyY + 2.0f);
    arms.quadraticTo(bodyX - 65.0f, bodyY + 11.0f - dance * level * 19.0f,
                     bodyX - 60.0f, bodyY - 8.0f - dance * level * 21.0f);
    arms.startNewSubPath(bodyX + 35.0f, bodyY + 2.0f);
    arms.quadraticTo(bodyX + 65.0f, bodyY + 11.0f + dance * level * 19.0f,
                     bodyX + 60.0f, bodyY - 8.0f + dance * level * 21.0f);
    g.strokePath(arms, stroke);

    g.setColour(mint.darker(0.10f));
    g.fillRoundedRectangle(bodyX - 30.0f,
                           bodyY + 32.0f + juce::jmax(0.0f, dance) * level * 7.0f,
                           24.0f, 16.0f, 8.0f);
    g.fillRoundedRectangle(bodyX + 6.0f,
                           bodyY + 32.0f + juce::jmax(0.0f, -dance) * level * 7.0f,
                           24.0f, 16.0f, 8.0f);

    // Two jaunty leaves make the little dancer a sprout.
    const float headTop = bodyY - bodyHeight * 0.5f;
    juce::Path stem;
    stem.startNewSubPath(bodyX, headTop + 7.0f);
    stem.quadraticTo(bodyX - 3.0f, headTop - 10.0f,
                     bodyX + dance * level * 5.0f, headTop - 19.0f);
    g.setColour(mint);
    g.strokePath(stem, juce::PathStrokeType(4.0f));

    juce::Path leaf;
    leaf.startNewSubPath(bodyX, headTop - 10.0f);
    leaf.cubicTo(bodyX - 29.0f, headTop - 6.0f,
                 bodyX - 30.0f, headTop - 32.0f,
                 bodyX - 28.0f, headTop - 33.0f);
    leaf.cubicTo(bodyX - 7.0f, headTop - 34.0f,
                 bodyX + 2.0f, headTop - 19.0f,
                 bodyX, headTop - 10.0f);
    leaf.closeSubPath();
    g.setColour(mint);
    g.fillPath(leaf);

    juce::Path otherLeaf;
    otherLeaf.startNewSubPath(bodyX + 1.0f, headTop - 13.0f);
    otherLeaf.cubicTo(bodyX + 4.0f, headTop - 33.0f,
                      bodyX + 22.0f, headTop - 34.0f,
                      bodyX + 30.0f, headTop - 29.0f);
    otherLeaf.cubicTo(bodyX + 24.0f, headTop - 9.0f,
                      bodyX + 10.0f, headTop - 8.0f,
                      bodyX + 1.0f, headTop - 13.0f);
    otherLeaf.closeSubPath();
    g.setColour(juce::Colour(0xffc2f5a6));
    g.fillPath(otherLeaf);

    const juce::Rectangle<float> body(
        bodyX - bodyWidth * 0.5f, headTop, bodyWidth, bodyHeight);
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xffb7ffdb), bodyX - 20.0f, headTop,
        juce::Colour(0xff62ceac), bodyX + 28.0f, body.getBottom(), false));
    g.fillRoundedRectangle(body, 36.0f);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.fillEllipse(bodyX - 27.0f, headTop + 12.0f, 17.0f, 8.0f);

    const float faceY = bodyY - 2.0f;
    const bool blink = std::fmod(time, 4.7f) > 4.53f;
    g.setColour(juce::Colour(0xff183737));
    for (int eye = 0; eye < 2; ++eye)
    {
        const float x = bodyX + (eye == 0 ? -16.0f : 16.0f);
        if (blink)
            g.drawLine(x - 4.0f, faceY, x + 4.0f, faceY, 2.5f);
        else
        {
            g.fillEllipse(x - 3.8f, faceY - 5.0f, 7.6f, 11.0f);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.fillEllipse(x - 1.4f, faceY - 3.0f, 2.2f, 2.8f);
            g.setColour(juce::Colour(0xff183737));
        }
    }

    g.setColour(pink.withAlpha(0.70f));
    g.fillEllipse(bodyX - 31.0f, faceY + 8.0f, 14.0f, 7.0f);
    g.fillEllipse(bodyX + 17.0f, faceY + 8.0f, 14.0f, 7.0f);

    juce::Path smile;
    smile.startNewSubPath(bodyX - 7.0f, faceY + 13.0f);
    smile.quadraticTo(bodyX, faceY + 22.0f + level * 3.0f,
                      bodyX + 7.0f, faceY + 13.0f);
    g.setColour(juce::Colour(0xff183737));
    g.strokePath(smile, juce::PathStrokeType(
        2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    setFont(g, 10.0f);
    g.setColour(muted);
    g.drawText(level > 0.03f ? "tiny sprout. big feelings." : "feed me a little music",
               bounds.toNearestInt().withY(static_cast<int>(bounds.getBottom()) - 24)
                                    .withHeight(14),
               juce::Justification::centred);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff243451).withAlpha(0.5f), 0.0f, 0.0f,
        background, 640.0f, 400.0f, false));
    g.fillRect(getLocalBounds());

    drawStar(g, 30.0f, 31.0f, 10.0f, mint);
    setFont(g, 25.0f, true);
    g.setColour(textColour);
    g.drawText("groove sprout", 49, 14, 280, 32, juce::Justification::centredLeft);

    setFont(g, 11.0f);
    g.setColour(muted);
    g.drawText("A tiny dancer for your biggest sounds.",
               20, 48, 350, 17, juce::Justification::centredLeft);

    g.setColour(mint.withAlpha(0.08f));
    g.fillRoundedRectangle(452.0f, 22.0f, 168.0f, 27.0f, 13.5f);
    g.setColour(mint.withAlpha(level > 0.02f ? 1.0f : 0.35f));
    g.fillEllipse(465.0f, 32.0f, 6.0f, 6.0f);
    setFont(g, 10.0f, true);
    g.setColour(mint);
    g.drawText("STEREO / AUDIO UNCHANGED",
               477, 22, 137, 27, juce::Justification::centredLeft);

    drawCreature(g, { 18.0f, 76.0f, 192.0f, 226.0f });
    drawSpectrum(g, { 220.0f, 76.0f, 402.0f, 96.0f });
    drawPitchTrails(g, { 220.0f, 182.0f, 402.0f, 120.0f });

    setFont(g, 14.0f, true);
    g.setColour(textColour);
    g.drawText("Your master, with a mascot.",
               20, 326, 280, 23, juce::Justification::centredLeft);

    setFont(g, 11.0f);
    g.setColour(muted);
    g.drawText("Screen-record the window. Share the groove.",
               20, 350, 290, 18, juce::Justification::centredLeft);
    g.drawText("Zero latency. All knobs are visual only.",
               20, 369, 290, 16, juce::Justification::centredLeft);
}
