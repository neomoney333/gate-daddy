#pragma once

#include "PluginProcessor.h"

class GateDaddyContent;

class GateDaddyEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int baseWidth = 1200, baseHeight = 832;

    explicit GateDaddyEditor (GateDaddyProcessor&);
    ~GateDaddyEditor() override;

    void resized() override;
    void setUiScale (float scale);

private:
    GateDaddyProcessor& proc;
    std::unique_ptr<GateDaddyContent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateDaddyEditor)
};
