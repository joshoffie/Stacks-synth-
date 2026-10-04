#pragma once

#include <JuceHeader.h>

namespace stacks
{

class StacksAudioProcessor;

// "Why it sounds like this, and which knob to touch": instant plain-words
// tips from the patch itself, then the model's own explanation as it streams.
class ExplainView : public juce::Component,
                    private juce::ChangeListener
{
public:
    explicit ExplainView (StacksAudioProcessor&);
    ~ExplainView() override;
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    StacksAudioProcessor& processor;
    juce::Label title;
    juce::TextButton explainButton;
    juce::TextEditor text;
    juce::String shown;
};

} // namespace stacks
