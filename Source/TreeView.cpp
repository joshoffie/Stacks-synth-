#include "TreeView.h"
#include "PluginProcessor.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"
#include "PatchGenerator.h"

namespace stacks
{

namespace
{
    constexpr float kColumnW = 128.0f, kRowH = 22.0f, kSeedH = 34.0f, kTop = 22.0f;
}

FamilyTree::FamilyTree (StacksAudioProcessor& p) : processor (p)
{
    setTooltip ("Every generation this session: the seed on top, its children below. Click any one to hear it again.");
}

int FamilyTree::preferredWidth() const
{
    return (int) (kColumnW * (float) (processor.lab().history.size() + 1)) + 20;
}

void FamilyTree::refresh()
{
    nodes.clear();
    const auto& lab = processor.lab();
    const int generations = (int) lab.history.size() + 1;
    for (int g = 0; g < generations; ++g)
    {
        const bool current = g == (int) lab.history.size();
        const auto& candidates = current ? lab.candidates : lab.history[(size_t) g].candidates;
        const auto& seed = current ? lab.seed : lab.history[(size_t) g].seed;
        const bool seedIsPatch = current ? lab.seedIsPatch : lab.history[(size_t) g].seedIsPatch;
        const float x = 10.0f + kColumnW * (float) g;

        Node s;
        s.bounds = { x, kTop, kColumnW - 12.0f, kSeedH };
        s.generation = g; s.candidate = -1; s.seed = true;
        s.name = seed.name.isNotEmpty() ? seed.name : juce::String ("Fresh ideas");
        s.tip = seedIsPatch ? "Seed of generation " + juce::String (g + 1) + ": " + seed.description : "Generation " + juce::String (g + 1) + " grew from a prompt" + (seed.prompt.isNotEmpty() ? ": " + seed.prompt : juce::String());
        s.ai = seedIsPatch;
        nodes.push_back (s);

        float y = kTop + kSeedH + 10.0f;
        for (int i = 0; i < (int) candidates.size(); ++i)
        {
            Node n;
            n.bounds = { x, y, kColumnW - 12.0f, kRowH - 2.0f };
            n.generation = g; n.candidate = i;
            n.name = candidates[(size_t) i].name;
            n.ai = candidates[(size_t) i].origin == "AI";
            n.tip = candidates[(size_t) i].description;
            if (seedIsPatch) n.tip << "\n" << countAudibleDifferences (candidates[(size_t) i], seed) << " audible changes from its seed";
            nodes.push_back (n);
            y += kRowH;
        }
    }
    setSize (preferredWidth(), juce::jmax (getHeight(), (int) (kTop + kSeedH + 10.0f + kRowH * 11.0f)));
    repaint();
}

void FamilyTree::paint (juce::Graphics& g)
{
    const auto& lab = processor.lab();
    const int generations = (int) lab.history.size() + 1;

    // Lines: the candidate that became the next generation's seed.
    for (int gen = 1; gen < generations; ++gen)
    {
        const Node* nextSeed = nullptr;
        for (const auto& n : nodes) if (n.generation == gen && n.seed) nextSeed = &n;
        if (nextSeed == nullptr) continue;
        for (const auto& n : nodes)
            if (n.generation == gen - 1 && ! n.seed && n.name == nextSeed->name)
            {
                juce::Path link;
                const auto a = n.bounds.getCentre().withX (n.bounds.getRight()), b = nextSeed->bounds.getCentre().withX (nextSeed->bounds.getX());
                link.startNewSubPath (a);
                link.cubicTo (a.x + 20.0f, a.y, b.x - 20.0f, b.y, b.x, b.y);
                g.setColour (colours::accent.withAlpha (0.7f));
                g.strokePath (link, juce::PathStrokeType (1.6f));
            }
    }

    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto& n = nodes[(size_t) i];
        const bool hot = i == hovered;
        if (n.seed)
        {
            g.setColour (hot ? colours::accentDim.brighter (0.2f) : colours::accentDim);
            g.fillRoundedRectangle (n.bounds, 6.0f);
            g.setColour (colours::accent);
            g.drawRoundedRectangle (n.bounds.reduced (0.5f), 6.0f, 1.2f);
            g.setColour (colours::muted);
            g.setFont (StacksLookAndFeel::font (9.0f, true));
            g.drawText ("GEN " + juce::String (n.generation + 1), n.bounds.withHeight (12.0f).translated (6.0f, 1.0f), juce::Justification::centredLeft);
            g.setColour (colours::text);
            g.setFont (StacksLookAndFeel::font (11.5f, true));
            g.drawFittedText (n.name, n.bounds.withTrimmedTop (12.0f).reduced (6.0f, 1.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        }
        else
        {
            const auto dotColour = n.ai ? colours::accent : juce::Colour (0xff4fb3a3);
            if (hot)
            {
                g.setColour (colours::card);
                g.fillRoundedRectangle (n.bounds, 4.0f);
            }
            g.setColour (dotColour);
            g.fillEllipse (n.bounds.getX() + 6.0f, n.bounds.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (hot ? colours::text : colours::text.withAlpha (0.8f));
            g.setFont (StacksLookAndFeel::font (11.0f));
            g.drawFittedText (n.name, n.bounds.withTrimmedLeft (20.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        }
    }

    if (nodes.size() <= 1)
    {
        g.setColour (colours::muted);
        g.setFont (StacksLookAndFeel::font (12.0f));
        g.drawFittedText ("The family tree fills in as you Generate and Evolve: each column is a generation, with its seed on top.",
                          getLocalBounds().reduced (20), juce::Justification::centred, 3);
    }
}

void FamilyTree::mouseMove (const juce::MouseEvent& e)
{
    int hit = -1;
    for (int i = 0; i < (int) nodes.size(); ++i)
        if (nodes[(size_t) i].bounds.contains (e.position)) hit = i;
    if (hit != hovered)
    {
        hovered = hit;
        setTooltip (hit >= 0 ? nodes[(size_t) hit].name + "\n" + nodes[(size_t) hit].tip : juce::String ("Every generation this session. Click any node to hear it again."));
        repaint();
    }
}

void FamilyTree::mouseDown (const juce::MouseEvent& e)
{
    for (const auto& n : nodes)
        if (n.bounds.contains (e.position))
        {
            processor.auditionFromTree (n.generation, n.candidate);
            return;
        }
}

} // namespace stacks
