#include "ExplainView.h"
#include "PluginProcessor.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

ExplainView::ExplainView (StacksAudioProcessor& p) : processor (p)
{
    title.setFont (StacksLookAndFeel::font (14.0f, true));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    explainButton.setButtonText ("Explain this sound");
    explainButton.setTooltip ("Ask the model why this patch sounds the way it does and what to try. The quick tips below never need the model.");
    explainButton.onClick = [this] { processor.explainCurrentPatch(); };
    addAndMakeVisible (explainButton);

    text.setMultiLine (true, true);
    text.setReadOnly (true);
    text.setScrollbarsShown (true);
    text.setCaretVisible (false);
    text.setFont (StacksLookAndFeel::font (12.5f));
    text.setColour (juce::TextEditor::textColourId, colours::text);
    text.setColour (juce::TextEditor::backgroundColourId, colours::panel);
    addAndMakeVisible (text);

    processor.labBroadcaster.addChangeListener (this);
    refresh();
}

ExplainView::~ExplainView()
{
    processor.labBroadcaster.removeChangeListener (this);
}

void ExplainView::refresh()
{
    const auto& lab = processor.lab();
    const auto current = processor.currentPatch();
    const bool forThis = lab.explanationKey == processor.currentExplanationKey();
    const bool hasModel = processor.aiBackend() != nullptr;

    title.setText (current.name, juce::dontSendNotification);
    explainButton.setEnabled (hasModel && ! (lab.explaining && forThis));
    explainButton.setButtonText (! hasModel ? "Pick an AI model in Settings for the model's view"
                                 : lab.explaining && forThis ? "Writing..." : forThis && lab.modelExplanation.isNotEmpty() ? "Explain again" : "Explain this sound");

    juce::String body;
    body << "QUICK TIPS\n\n" << (forThis && lab.explanation.isNotEmpty() ? lab.explanation : patchTips (current));
    if (hasModel)
    {
        body << "\n\n\nFROM " << processor.engineName().toUpperCase() << "\n\n";
        if (forThis && lab.modelExplanation.isNotEmpty()) body << lab.modelExplanation;
        else if (forThis && lab.explaining)                body << "Listening to the patch and writing...";
        else                                               body << "Press \"Explain this sound\" for the model's take: why it sounds like this, and three knob moves to try.";
    }
    if (body != shown)
    {
        shown = body;
        text.setText (body, false);
    }
}

void ExplainView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (26);
    explainButton.setBounds (top.removeFromRight (170));
    top.removeFromRight (6);
    title.setBounds (top);
    r.removeFromTop (6);
    text.setBounds (r);
}

void ExplainView::paint (juce::Graphics&) {}

} // namespace stacks
