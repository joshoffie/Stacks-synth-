#include "TreeCanvas.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"
#include "Skin.h"

namespace stacks
{

namespace
{
    class NodeHandler : public juce::AccessibilityHandler
    {
    public:
        NodeHandler (juce::Component& c, std::function<void()> press)
            : juce::AccessibilityHandler (c, juce::AccessibilityRole::button,
                                          juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, std::move (press))) {}
        juce::String getTitle() const override { return getComponent().getTitle(); }
    };
}

//==============================================================================
// One block on the canvas. Click to hear it; the plus in the corner (or a
// Cmd-click) puts it in the mix to grow from.
class TreeCanvas::NodeComp : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    NodeComp (TreeCanvas& o, int index) : owner (o), node (index)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    juce::Rectangle<float> plusRect() const
    {
        return juce::Rectangle<float> (20.0f, 20.0f).withPosition (face.getRight() - 24.0f, face.getY() + 4.0f);
    }

    void paint (juce::Graphics& g) override
    {
        if (node < 0 || node >= (int) owner.nodes.size()) return;
        const auto& n = owner.nodes[(size_t) node];
        const bool lit = n.cand >= 0 && owner.processor.lab().auditioned == n.cand && owner.columns.back().gen == n.gen;
        const bool selected = owner.isSelected (n);
        g.setOpacity (juce::jmin (1.0f, n.age * 4.0f));
        if (n.cand < 0)
        {
            auto s = n.style; s.wave.clear(); s.depth = kDepth * owner.zoom;
            GardenView::paintBlock (g, face, s, n.name, false, hovered, node);
        }
        else
            GardenView::paintBlock (g, face, n.style, n.name, lit, hovered, node);
        if (selected)
        {
            g.setColour (colours::accent);
            g.drawRoundedRectangle (face.expanded (3.0f), 6.0f, 2.0f);
        }
        if ((hovered || selected) && n.cand >= 0)
        {
            const auto r = plusRect();
            g.setColour (selected ? colours::accent : juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (r);
            g.setColour (selected ? juce::Colours::black : colours::text);
            g.setFont (StacksLookAndFeel::font (15.0f, true));
            g.drawText (selected ? juce::String::fromUTF8 ("\xe2\x9c\x93") : "+", r, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (node < 0 || node >= (int) owner.nodes.size()) return;
        const auto& n = owner.nodes[(size_t) node];
        if (n.cand >= 0 && (e.mods.isCommandDown() || e.mods.isShiftDown() || plusRect().expanded (3.0f).contains (e.position)))
            owner.toggleSelected (n);
        else
            owner.hear (n);
    }
    void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hovered = false; repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<NodeHandler> (*this, [this] { if (node >= 0 && node < (int) owner.nodes.size()) owner.hear (owner.nodes[(size_t) node]); });
    }

    TreeCanvas& owner;
    int node;
    juce::Rectangle<float> face;   // in this component's space
    bool hovered = false;
};

//==============================================================================
TreeCanvas::TreeCanvas (StacksAudioProcessor& p, float z) : processor (p), zoom (z), defaultZoom (z)
{
    setOpaque (false);
    centreButton.setTooltip ("Back to the newest sounds");
    centreButton.onClick = [this] { centreOnNewest (true); };
    centreButton.setWantsKeyboardFocus (false);
    addChildComponent (centreButton);
    refresh();
}

TreeCanvas::~TreeCanvas() { stopTimer(); }

int TreeCanvas::nodeAt (int gen, int cand) const
{
    for (int i = 0; i < (int) nodes.size(); ++i)
        if (nodes[(size_t) i].gen == gen && nodes[(size_t) i].cand == cand) return i;
    return -1;
}

// Nodes and columns from the lab state: every remembered generation and the
// one in progress. The first time a batch is seen its blocks drop in; a
// restored tree sits still.
void TreeCanvas::rebuild (bool settledOnly)
{
    const auto& lab = processor.lab();
    const int generations = (int) lab.history.size() + 1;
    std::vector<Node> fresh;
    std::vector<Column> cols;
    auto previous = std::move (nodes);

    for (int g = 0; g < generations; ++g)
    {
        const bool current = g == (int) lab.history.size();
        const auto& cands = current ? lab.candidates : lab.history[(size_t) g].candidates;
        const auto& seed = current ? lab.seed : lab.history[(size_t) g].seed;
        const bool seedIsPatch = current ? lab.seedIsPatch : lab.history[(size_t) g].seedIsPatch;
        const auto& parentRefs = current ? lab.currentParents : lab.history[(size_t) g].parents;
        if (cands.empty() && ! current) continue;

        Column col;
        col.gen = g;
        col.current = current;
        // Parents: recorded references, else the earlier node with the seed's name, else a seed slab of its own.
        for (const auto& ref : parentRefs)
            if (ref.first < g)
            {
                for (int i = 0; i < (int) fresh.size(); ++i)
                    if (fresh[(size_t) i].gen == ref.first && fresh[(size_t) i].cand == ref.second) col.parents.push_back (i);
            }
        if (col.parents.empty() && seedIsPatch)
            for (int i = (int) fresh.size() - 1; i >= 0; --i)
                if (fresh[(size_t) i].cand >= 0 && fresh[(size_t) i].name == seed.name) { col.parents.push_back (i); break; }
        if (col.parents.empty())
        {
            Node s;
            s.gen = g; s.cand = -1;
            s.name = seed.name.isNotEmpty() ? seed.name : "Fresh ideas";
            s.description = seedIsPatch ? seed.description : juce::String ("What this batch grew from");
            s.style = seedIsPatch ? GardenView::styleFor (seed, processor.builtInWavetables(), true) : GardenView::Style();
            if (! seedIsPatch) s.style.colour = colours::card.brighter (0.25f);
            s.style.depth = kDepth; s.style.width = 1.0f;
            col.seedNode = (int) fresh.size();
            fresh.push_back (s);
        }
        for (int c = 0; c < (int) cands.size(); ++c)
        {
            Node n;
            n.gen = g; n.cand = c;
            n.name = cands[(size_t) c].name;
            n.description = cands[(size_t) c].description.upToFirstOccurrenceOf (" (", false, false);
            n.style = GardenView::styleFor (cands[(size_t) c], processor.builtInWavetables(), true);
            n.style.depth = kDepth; n.style.width = 1.0f;
            // a block seen before keeps its animation state; a new one in the current batch drops in
            bool known = false;
            for (const auto& old : previous)
                if (old.gen == g && old.cand == c) { n.drop = old.drop; n.vy = old.vy; n.scale = old.scale; n.age = old.age; n.landed = old.landed; known = true; break; }
            if (! known && current && ! settledOnly) { n.drop = -260.0f; n.vy = 0.0f; n.scale = 0.3f; n.age = 0.0f; n.landed = false; }
            col.nodes.push_back ((int) fresh.size());
            fresh.push_back (n);
        }
        cols.push_back (col);
    }
    nodes = std::move (fresh);
    columns = std::move (cols);
    layout();
}

// World positions: columns left to right, each batch centred on the sounds it grew from.
void TreeCanvas::layout()
{
    float x = 40.0f;
    for (auto& col : columns)
    {
        float cy = 0.0f;
        if (! col.parents.empty())
        {
            for (int p : col.parents) cy += nodes[(size_t) p].target.y + kBlockH * 0.5f;
            cy /= (float) col.parents.size();
        }
        if (col.seedNode >= 0)
        {
            nodes[(size_t) col.seedNode].target = { x, cy - kBlockH * 0.5f };
            x += kColW * 0.9f;
        }
        col.x = x;
        const int n = juce::jmax ((int) col.nodes.size(), col.current && processor.lab().generating ? StacksAudioProcessor::kAiPatchesPerBatch : 0);
        for (int i = 0; i < (int) col.nodes.size(); ++i)
            nodes[(size_t) col.nodes[(size_t) i]].target = { x, cy + ((float) i - (float) (n - 1) * 0.5f) * kPitch - kBlockH * 0.5f };
        x += kColW;
    }
}

void TreeCanvas::refresh()
{
    const auto& lab = processor.lab();
    const int gens = (int) lab.history.size(), cands = (int) lab.candidates.size();
    const bool structural = gens != knownGenerations;
    rebuild (knownGenerations < 0);   // the first fill (a restored session) sits still
    if (structural && knownGenerations >= 0 && centredWithSize) centreOnNewest (true);
    if (knownGenerations < 0 && getWidth() > 0) { centreOnNewest (false); centredWithSize = true; }
    knownGenerations = gens; knownCandidates = cands;
    placeNodes();
    bool moving = animatingPan || lab.generating;
    for (const auto& n : nodes) moving = moving || ! n.landed || n.scale < 0.995f || n.age < 0.3f;
    if (moving && isVisible()) { if (! isTimerRunning()) startTimerHz (30); }
    repaint();
}

void TreeCanvas::centreOnNewest (bool animate)
{
    if (columns.empty()) return;
    const auto& col = columns.back();
    float cy = 0.0f; int count = 0;
    for (int i : col.nodes) { cy += nodes[(size_t) i].target.y + kBlockH * 0.5f; ++count; }
    if (count == 0 && col.seedNode >= 0) { cy = nodes[(size_t) col.seedNode].target.y + kBlockH * 0.5f; count = 1; }
    if (count > 0) cy /= (float) count;
    const float w = (float) juce::jmax (1, getWidth()) / zoom, h = (float) juce::jmax (1, getHeight()) / zoom;
    const float cx = col.x + kBlockW * 0.5f;
    panTarget = { cx - w * 0.62f, cy - h * 0.5f };
    if (! animate) pan = panTarget;
    animatingPan = animate;
    if (animate && isVisible() && ! isTimerRunning()) startTimerHz (30);
    placeNodes();
}

void TreeCanvas::clearSelection()
{
    if (mix.empty()) return;
    mix.clear();
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void TreeCanvas::toggleSelected (const Node& n)
{
    const std::pair<int, int> key { n.gen, n.cand };
    if (mix.count (key)) mix.erase (key); else mix.insert (key);
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void TreeCanvas::hear (const Node& n)
{
    processor.auditionFromTree (n.gen, n.cand);
}

juce::Point<float> TreeCanvas::toScreen (juce::Point<float> world) const
{
    return { (world.x - pan.x) * zoom, (world.y - pan.y) * zoom };
}

juce::Rectangle<float> TreeCanvas::faceOf (const Node& n) const
{
    const auto p = toScreen ({ n.target.x, n.target.y + n.drop });
    const float w = kBlockW * zoom * n.scale, h = kBlockH * zoom * n.scale;
    return { p.x + (kBlockW * zoom - w) * 0.5f, p.y + kBlockH * zoom - h, w, h };
}

void TreeCanvas::placeNodes()
{
    while (comps.size() < nodes.size()) { auto c = std::make_unique<NodeComp> (*this, (int) comps.size()); addAndMakeVisible (*c); comps.push_back (std::move (c)); }
    while (comps.size() > nodes.size()) comps.pop_back();
    const float depth = kDepth * zoom + 2.0f, skew = GardenView::kBlockSkew * zoom;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto& n = nodes[i];
        auto& c = *comps[i];
        n.style.depth = kDepth * zoom;
        const auto face = faceOf (n);
        juce::Rectangle<float> bounds (face.getX() - 4.0f, face.getY() - depth, face.getWidth() + skew + 8.0f, face.getHeight() + depth + 6.0f);
        const auto ib = bounds.toNearestInt();
        c.setBounds (ib);
        c.face = face.translated (-(float) ib.getX(), -(float) ib.getY());
        c.node = (int) i;
        if (c.getTitle() != "Hear " + n.name) { c.setTitle ("Hear " + n.name); c.setTooltip (n.description.isNotEmpty() ? n.name + ": " + n.description : n.name); c.invalidateAccessibilityHandler(); }
        c.setVisible (ib.intersects (getLocalBounds().expanded (40)));
        c.repaint();
    }
    const bool away = pan.getDistanceFrom (panTarget) > 2.0f || (! columns.empty() && std::abs (toScreen ({ columns.back().x, 0.0f }).x - (float) getWidth() * 0.62f) > (float) getWidth() * 0.6f);
    centreButton.setVisible (away && ! animatingPan);
    centreButton.setBounds (getWidth() - 74, 6, 68, 22);
}

void TreeCanvas::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    anim += dt;
    bool moving = false;
    if (animatingPan)
    {
        pan += (panTarget - pan) * juce::jmin (1.0f, 8.0f * dt);
        if (pan.getDistanceFrom (panTarget) < 0.5f) { pan = panTarget; animatingPan = false; }
        moving = true;
    }
    for (auto& n : nodes)
    {
        n.age += dt;
        if (! n.landed)
        {
            n.vy += 2600.0f * dt;
            n.drop += n.vy * dt;
            n.scale += (1.05f - n.scale) * juce::jmin (1.0f, 7.0f * dt);
            if (n.drop >= 0.0f)
            {
                n.drop = 0.0f;
                if (n.vy > 160.0f) n.vy = -n.vy * 0.3f; else { n.vy = 0.0f; n.landed = true; }
            }
            moving = true;
        }
        else if (n.scale < 0.995f || n.scale > 1.005f) { n.scale += (1.0f - n.scale) * juce::jmin (1.0f, 6.0f * dt); moving = true; }
        if (n.age < 0.3f) moving = true;
    }
    if (processor.lab().generating) { rebuild (false); moving = true; }
    placeNodes();
    if (! moving) stopTimer();
    repaint();
}

void TreeCanvas::visibilityChanged()
{
    if (isVisible()) refresh(); else stopTimer();
}

void TreeCanvas::resized()
{
    if (! centredWithSize && getWidth() > 0 && getHeight() > 0) { centredWithSize = true; centreOnNewest (false); return; }
    placeNodes();
}

//==============================================================================
void TreeCanvas::paint (juce::Graphics& g)
{
    const auto& lab = processor.lab();
    const float bw = kBlockW * zoom, bh = kBlockH * zoom;

    // Connectors: from each parent's right edge to a bus, then into every child.
    for (const auto& col : columns)
    {
        std::vector<int> children = col.nodes;
        const bool growing = col.current && lab.generating;
        if (col.parents.empty() && col.seedNode < 0) continue;
        std::vector<juce::Point<float>> sources;
        if (col.seedNode >= 0) { const auto f = toScreen (nodes[(size_t) col.seedNode].target); sources.push_back ({ f.x + bw + GardenView::kBlockSkew * zoom, f.y + bh * 0.5f }); }
        for (int p : col.parents) { const auto f = toScreen (nodes[(size_t) p].target); sources.push_back ({ f.x + bw + GardenView::kBlockSkew * zoom, f.y + bh * 0.5f }); }
        const int slots = growing ? juce::jmax ((int) children.size(), StacksAudioProcessor::kAiPatchesPerBatch) : (int) children.size();
        if (slots == 0) continue;
        const float busX = toScreen ({ col.x, 0.0f }).x - 22.0f * zoom;
        float top = 1e9f, bottom = -1e9f;
        auto slotY = [&] (int i)
        {
            if (i < (int) children.size()) return toScreen (nodes[(size_t) children[(size_t) i]].target).y + bh * 0.5f;
            // a waiting slot below the ones that have landed
            float cy = 0.0f;
            for (const auto& s : sources) cy += s.y;
            cy /= (float) juce::jmax (1, (int) sources.size());
            return cy + ((float) i - (float) (slots - 1) * 0.5f) * kPitch * zoom;
        };
        for (int i = 0; i < slots; ++i) { top = juce::jmin (top, slotY (i)); bottom = juce::jmax (bottom, slotY (i)); }
        const bool highlighted = col.current;
        g.setColour (highlighted ? colours::accent.withAlpha (0.75f) : colours::muted.withAlpha (0.55f));
        const float stroke = juce::jmax (1.0f, 1.6f * zoom);
        for (const auto& s : sources)
        {
            juce::Path elbow;
            elbow.startNewSubPath (s);
            const float midX = s.x + (busX - s.x) * 0.5f;
            elbow.lineTo (midX, s.y);
            elbow.lineTo (midX, juce::jlimit (top, bottom, s.y));
            elbow.lineTo (busX, juce::jlimit (top, bottom, s.y));
            g.strokePath (elbow, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (slots > 1) g.drawLine (busX, top, busX, bottom, stroke);
        for (int i = 0; i < slots; ++i)
        {
            const float y = slotY (i);
            const float x1 = toScreen ({ col.x, 0.0f }).x - 4.0f;
            g.drawLine (busX, y, x1, y, stroke);
        }
        // Waiting slots while the batch grows: a faint frame for each sound still to come, the next one lit.
        if (growing)
            for (int i = (int) children.size(); i < slots; ++i)
            {
                const auto p = toScreen ({ col.x, 0.0f });
                juce::Rectangle<float> slot (p.x, slotY (i) - bh * 0.5f, bw, bh);
                const bool next = i == (int) children.size();
                const float pulse = 0.5f + 0.5f * std::sin (anim * 3.0f);
                g.setColour (next ? colours::accent.withAlpha (0.35f + 0.4f * pulse) : colours::muted.withAlpha (0.18f));
                const float dashes[] = { 5.0f, 4.0f };
                juce::Path outline, dashed;
                outline.addRoundedRectangle (slot, 4.0f);
                juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
                g.fillPath (dashed);
                if (next)
                {
                    const float progress = lab.progress > 0.01f ? juce::jlimit (0.0f, 1.0f, lab.progress) : std::fmod (anim * 0.16f, 1.0f);
                    g.setColour (colours::accent.withAlpha (0.18f));
                    g.fillRoundedRectangle (slot.withTrimmedTop (slot.getHeight() * (1.0f - progress)), 4.0f);
                    // the settings being written, beside the slot
                    const auto& writes = lab.liveWrites;
                    if (! writes.isEmpty() && zoom > 0.6f)
                    {
                        g.setFont (StacksLookAndFeel::font (11.0f * juce::jmin (1.2f, zoom)));
                        auto lines = juce::Rectangle<float> (slot.getRight() + 24.0f * zoom, slot.getY() - 20.0f, 220.0f * zoom, 100.0f);
                        const int shown = juce::jmin (5, writes.size());
                        for (int k = 0; k < shown; ++k)
                        {
                            const float age = (float) (shown - 1 - k) / (float) juce::jmax (1, shown - 1);
                            g.setColour (colours::text.withAlpha (0.85f - 0.6f * age));
                            g.drawText (writes[writes.size() - shown + k], lines.removeFromTop (15.0f * juce::jmin (1.2f, zoom)), juce::Justification::centredLeft);
                        }
                    }
                }
            }
        g.setColour (highlighted ? colours::accent.withAlpha (0.75f) : colours::muted.withAlpha (0.55f));
    }
}

//==============================================================================
void TreeCanvas::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    dragStartPan = pan;
    animatingPan = false;
    (void) e;
}

void TreeCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    pan = dragStartPan - e.getOffsetFromDragStart().toFloat() / zoom;
    placeNodes();
    repaint();
}

void TreeCanvas::mouseUp (const juce::MouseEvent&) { dragging = false; placeNodes(); }

void TreeCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown()) { mouseMagnify (e, 1.0f + w.deltaY); return; }
    const float step = w.isSmooth ? 260.0f : 80.0f;
    pan -= juce::Point<float> (w.deltaX, w.deltaY) * step / zoom;
    animatingPan = false;
    placeNodes();
    repaint();
}

void TreeCanvas::mouseMagnify (const juce::MouseEvent& e, float factor)
{
    const float before = zoom;
    zoom = juce::jlimit (0.45f, 1.6f, zoom * factor);
    // keep the point under the mouse still
    const auto m = e.position;
    pan = { pan.x + m.x / before - m.x / zoom, pan.y + m.y / before - m.y / zoom };
    animatingPan = false;
    placeNodes();
    repaint();
}

std::vector<std::pair<juce::String, juce::Rectangle<int>>> TreeCanvas::skinPieces() const
{
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> out;
    for (size_t i = 0; i < nodes.size() && i < comps.size(); ++i)
    {
        if (nodes[i].cand < 0 && std::none_of (out.begin(), out.end(), [] (const auto& p) { return p.first == "slab.png"; })) out.emplace_back ("slab.png", comps[i]->getBounds());
        if (nodes[i].cand >= 0 && std::none_of (out.begin(), out.end(), [] (const auto& p) { return p.first == "block.png"; })) out.emplace_back ("block.png", comps[i]->getBounds());
    }
    return out;
}

juce::Rectangle<int> TreeCanvas::blocksArea() const
{
    juce::Rectangle<int> r;
    if (columns.empty()) return getLocalBounds();
    for (int i : columns.back().nodes) r = r.isEmpty() ? comps[(size_t) i]->getBounds() : r.getUnion (comps[(size_t) i]->getBounds());
    return r.isEmpty() ? getLocalBounds() : r;
}

} // namespace stacks
