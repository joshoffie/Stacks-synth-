#include "GardenView.h"
#include "Controls.h"

namespace stacks
{

namespace
{
    constexpr float kSeedRadius = 38.0f;
    constexpr float kLeafRadius = 11.0f;
    constexpr double kSproutMs = 420.0;
    const juce::Colour kAiLeaf     { 0xfff2a541 };
    const juce::Colour kRandomLeaf { 0xff5ec8c0 };
    juce::String heart() { return juce::String::fromUTF8 ("\xe2\x99\xa5"); }
}

//==============================================================================
class GardenView::Leaf : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    Leaf (GardenView& g, int idx) : garden (g), index (idx) {}

    void set (const Patch& p, bool isAuditioned, bool isFavourite, float grow)
    {
        name = p.name;
        ai = p.origin == "AI";
        auditioned = isAuditioned;
        favourite = isFavourite;
        growth = grow;
        setTooltip (p.name + (p.category.isNotEmpty() ? "  (" + p.category + ")" : "") + "\n" + p.description
                    + "\n\nclick: hear   drag outward: wilder   right-click: plant / favourite");
        setTitle ("Audition " + name);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto centre = getLocalBounds().toFloat().getCentre();
        const float r = (auditioned ? kLeafRadius + 3.0f : kLeafRadius) * juce::jlimit (0.05f, 1.0f, growth);
        const auto colour = ai ? kAiLeaf : kRandomLeaf;

        if (hovered || auditioned)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillEllipse (centre.x - r - 6.0f, centre.y - r - 6.0f, 2.0f * (r + 6.0f), 2.0f * (r + 6.0f));
        }
        g.setColour (ai ? colour : colour.withAlpha (0.75f));
        g.fillEllipse (centre.x - r, centre.y - r, 2.0f * r, 2.0f * r);
        if (auditioned)
        {
            g.setColour (colours::text);
            g.drawEllipse (centre.x - r - 1.5f, centre.y - r - 1.5f, 2.0f * r + 3.0f, 2.0f * r + 3.0f, 1.8f);
        }
        if (favourite)
        {
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            g.drawText (heart(), getLocalBounds(), juce::Justification::centred, false);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); garden.hoveredLeaf = index; garden.repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hovered = false; repaint(); if (garden.hoveredLeaf == index) garden.hoveredLeaf = -1; garden.repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem ("Plant: evolve from this", [this] { if (garden.onEvolveFrom) garden.onEvolveFrom (index); });
            menu.addItem ((favourite ? "Un-favourite" : heart() + "  Favourite"), [this] { garden.processor.toggleFavourite (index); });
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
            return;
        }
        garden.processor.audition (index);
        dragStart = e.getEventRelativeTo (&garden).position;
        dragVariation = garden.getVariation ? garden.getVariation() : 0.5f;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Pull a leaf outward for wilder children, inward for tamer ones.
        if (! garden.setVariation) return;
        const auto pos = e.getEventRelativeTo (&garden).position;
        const auto centre = garden.seedCentre();
        const float d0 = dragStart.getDistanceFrom (centre), d1 = pos.getDistanceFrom (centre);
        garden.setVariation (juce::jlimit (0.0f, 1.0f, dragVariation + (d1 - d0) / 120.0f));
        garden.layoutLeaves();
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { garden.processor.audition (index); }));
    }

    GardenView& garden;
    int index;
    juce::String name;
    bool ai = false, auditioned = false, favourite = false, hovered = false;
    float growth = 1.0f;
    juce::Point<float> dragStart;
    float dragVariation = 0.5f;
};

//==============================================================================
GardenView::GardenView (StacksAudioProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

GardenView::~GardenView()
{
    stopTimer();
}

juce::Point<float> GardenView::seedCentre() const
{
    return { getWidth() * 0.5f, getHeight() * 0.5f - 6.0f };
}

float GardenView::branchLength() const
{
    const float room = juce::jmin (getWidth(), getHeight()) * 0.5f - 30.0f;
    const float v = getVariation ? getVariation() : 0.5f;
    return juce::jlimit (kSeedRadius + 30.0f, room, kSeedRadius + 40.0f + v * (room - kSeedRadius - 40.0f));
}

void GardenView::refresh()
{
    const auto& lab = processor.lab();
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (lab.generation != shownGeneration)
    {
        shownGeneration = lab.generation;
        appearedAt.clear();
    }
    while (appearedAt.size() < lab.candidates.size())
        appearedAt.push_back (now);
    appearedAt.resize (lab.candidates.size());

    while (leaves.size() < lab.candidates.size())
    {
        auto leaf = std::make_unique<Leaf> (*this, (int) leaves.size());
        addAndMakeVisible (*leaf);
        leaves.push_back (std::move (leaf));
    }
    while (leaves.size() > lab.candidates.size())
        leaves.pop_back();

    layoutLeaves();
    startTimerHz (30);
    repaint();
}

void GardenView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    bool animating = processor.lab().generating;
    for (double t : appearedAt)
        if (now - t < kSproutMs) animating = true;
    layoutLeaves();
    repaint();
    if (! animating)
        stopTimer();
}

// AI leaves spread over the upper half, random ones over the lower half.
void GardenView::layoutLeaves()
{
    const auto& lab = processor.lab();
    const auto centre = seedCentre();
    const float length = branchLength();
    const double now = juce::Time::getMillisecondCounterHiRes();

    int aiCount = 0, randomCount = 0;
    for (const auto& c : lab.candidates) (c.origin == "AI" ? aiCount : randomCount)++;
    const int aiTotal = juce::jmax (aiCount, lab.generating ? StacksAudioProcessor::kAiPatchesPerBatch : aiCount);

    int aiIndex = 0, randomIndex = 0;
    for (int i = 0; i < (int) leaves.size(); ++i)
    {
        const auto& c = lab.candidates[(size_t) i];
        const bool ai = c.origin == "AI";
        float angle; // 0 = right, going anticlockwise (screen y flipped below)
        if (ai)
        {
            const float t = (aiIndex + 1.0f) / (aiTotal + 1.0f);
            angle = juce::MathConstants<float>::pi * (1.0f - t);          // 180° -> 0° across the top
            ++aiIndex;
        }
        else
        {
            const float t = (randomIndex + 1.0f) / (randomCount + 1.0f);
            angle = juce::MathConstants<float>::pi * (1.0f + t);          // 180° -> 360° across the bottom
            ++randomIndex;
        }
        const float grow = (float) juce::jlimit (0.0, 1.0, (now - appearedAt[(size_t) i]) / kSproutMs);
        const float eased = 1.0f - (1.0f - grow) * (1.0f - grow);
        const float len = (ai ? length : length * 0.82f) * eased;
        const juce::Point<float> pos (centre.x + std::cos (angle) * len, centre.y - std::sin (angle) * len);

        auto& leaf = *leaves[(size_t) i];
        const int size = (int) (kLeafRadius + 9.0f) * 2;
        leaf.setBounds ((int) pos.x - size / 2, (int) pos.y - size / 2, size, size);
        leaf.set (c, i == lab.auditioned, c.favourite, eased);
    }
}

void GardenView::resized()
{
    layoutLeaves();
}

void GardenView::mouseDown (const juce::MouseEvent& e)
{
    const auto centre = seedCentre();
    if (e.position.getDistanceFrom (centre) <= kSeedRadius)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem ("Fresh ideas (new generation from scratch)", [this] { if (onFresh) onFresh(); });
            menu.addItem ("Evolve this sound", [this] { if (onEvolve) onEvolve(); });
            menu.addItem (heart() + "  Favourite this sound", [this] { processor.favouriteCurrent(); });
            menu.showMenuAsync (juce::PopupMenu::Options());
        }
        else if (processor.lab().seedIsPatch)
        {
            processor.auditionSeed();   // hear the parent of this generation again
        }
    }
}


void GardenView::paint (juce::Graphics& g)
{
    const auto& lab = processor.lab();
    const auto centre = seedCentre();
    // The seed is what this generation grew from, not whatever is loaded now.
    const auto seedLabel = lab.generation > 0 && lab.seed.name.isNotEmpty() ? lab.seed.name : processor.currentPatchName();

    // Branches
    for (int i = 0; i < (int) leaves.size(); ++i)
    {
        const auto& leaf = *leaves[(size_t) i];
        const auto end = leaf.getBounds().toFloat().getCentre();
        const auto dir = (end - centre);
        if (dir.getDistanceFromOrigin() < 1.0f) continue;
        const auto unit = dir / dir.getDistanceFromOrigin();
        const auto start = centre + unit * kSeedRadius;
        g.setColour ((leaf.ai ? kAiLeaf : kRandomLeaf).withAlpha (i == lab.auditioned || i == hoveredLeaf ? 0.9f : leaf.ai ? 0.45f : 0.3f));
        g.drawLine (juce::Line<float> (start, end - unit * kLeafRadius * leaf.growth), i == lab.auditioned ? 2.2f : 1.4f);
    }

    // Seed
    const bool generating = lab.generating;
    const float pulse = generating ? 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.004f) : 0.0f;
    g.setColour (colours::accent.withAlpha (0.12f + 0.18f * pulse));
    g.fillEllipse (centre.x - kSeedRadius - 8.0f, centre.y - kSeedRadius - 8.0f, 2.0f * (kSeedRadius + 8.0f), 2.0f * (kSeedRadius + 8.0f));
    g.setColour (colours::card);
    g.fillEllipse (centre.x - kSeedRadius, centre.y - kSeedRadius, 2.0f * kSeedRadius, 2.0f * kSeedRadius);
    g.setColour (colours::accent);
    g.drawEllipse (centre.x - kSeedRadius, centre.y - kSeedRadius, 2.0f * kSeedRadius, 2.0f * kSeedRadius, 1.6f);

    // Progress ring while a generation grows: completed leaves plus the one being written.
    if (generating && processor.engine().kind != EngineKind::Random)
    {
        int aiDone = 0;
        for (const auto& c : lab.candidates) if (c.origin == "AI") ++aiDone;
        const float total = (float) StacksAudioProcessor::kAiPatchesPerBatch;
        const float frac = juce::jlimit (0.0f, 1.0f, ((float) aiDone + juce::jmax (0.0f, lab.progress)) / total);
        const float rr = kSeedRadius + 5.0f;
        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, rr, rr, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * frac, true);
        g.setColour (colours::accent.withAlpha (0.9f));
        g.strokePath (ring, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (lab.progress < 0.0f)
        {
            const float t = (float) std::fmod (juce::Time::getMillisecondCounterHiRes() / 1200.0, 1.0) * juce::MathConstants<float>::twoPi;
            juce::Path sweep;
            sweep.addCentredArc (centre.x, centre.y, rr, rr, 0.0f, t, t + 0.9f, true);
            g.setColour (colours::text.withAlpha (0.6f));
            g.strokePath (sweep, juce::PathStrokeType (3.0f));
        }
    }
    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawFittedText (seedLabel, juce::Rectangle<float> (centre.x - kSeedRadius + 4.0f, centre.y - kSeedRadius + 6.0f,
                                                                           2.0f * kSeedRadius - 8.0f, 2.0f * kSeedRadius - 12.0f).toNearestInt(),
                      juce::Justification::centred, 3, 0.8f);

    // Leaf names
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (int i = 0; i < (int) leaves.size(); ++i)
    {
        const auto& leaf = *leaves[(size_t) i];
        if (leaf.growth < 0.6f) continue;
        const auto pos = leaf.getBounds().toFloat().getCentre();
        const auto dir = pos - centre;
        const auto unit = dir / juce::jmax (1.0f, dir.getDistanceFromOrigin());
        const auto anchor = pos + unit * (kLeafRadius + 6.0f);
        const bool right = unit.x >= 0.0f;
        juce::Rectangle<float> box (right ? anchor.x : anchor.x - 90.0f, anchor.y - 8.0f, 90.0f, 16.0f);
        g.setColour (i == lab.auditioned ? colours::text : colours::muted);
        g.drawFittedText (leaf.name, box.toNearestInt(), right ? juce::Justification::centredLeft : juce::Justification::centredRight, 1);
    }

    // Caption / hover description
    auto footer = getLocalBounds().removeFromBottom (34).reduced (6, 0);
    g.setColour (colours::muted);
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    juce::String text;
    if (hoveredLeaf >= 0 && hoveredLeaf < (int) lab.candidates.size())
        text = lab.candidates[(size_t) hoveredLeaf].description;
    else if (generating)
        text = lab.progressDetail.isNotEmpty() ? lab.progressDetail : "growing...";
    else if (lab.candidates.empty())
        text = "Press Fresh ideas to grow the first leaves. Click a leaf to hear it, drag it outward for wilder children.";
    else
        text = lab.seedIsPatch ? "click a leaf: hear   -   click the seed: hear the parent again   -   drag a leaf outward: wilder   -   right-click a leaf: plant it"
                               : "click a leaf: hear   -   drag a leaf outward: wilder   -   right-click a leaf: plant it   -   Evolve grows from what you're hearing";
    g.drawFittedText (text, footer, juce::Justification::centred, 2, 0.9f);

    if (lab.generation > 0)
    {
        g.setColour (colours::muted);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("GEN " + juce::String (lab.generation), getLocalBounds().removeFromTop (14).withTrimmedLeft (6), juce::Justification::centredLeft);
    }
}

} // namespace stacks
