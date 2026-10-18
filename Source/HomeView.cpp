#include "HomeView.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    struct Kind { const char* name; const char* blurb; juce::uint32 colour; };
    const Kind kKinds[] = {
        { "Pad",     "soft and sustained, for chords",       0xff7fa7d8 },
        { "Keys",    "pianos, electric pianos, organs",      0xffe0c070 },
        { "Bass",    "the low end",                          0xffe8775a },
        { "Lead",    "melodies on top",                      0xfff2a541 },
        { "Pluck",   "short and bouncy",                     0xff5ec8c0 },
        { "Bell",    "glassy and ringing",                   0xffd08ab8 },
        { "Texture", "atmospheres, noise, drones",           0xff9ad17b },
    };

    class PressHandler : public juce::AccessibilityHandler
    {
    public:
        PressHandler (juce::Component& c, const juce::String& title, std::function<void()> press)
            : juce::AccessibilityHandler (c, juce::AccessibilityRole::button,
                                          juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, std::move (press))),
              titleText (title) {}
        juce::String getTitle() const override { return getComponent().getTitle().isNotEmpty() ? getComponent().getTitle() : titleText; }
    private:
        juce::String titleText;
    };
}

//==============================================================================
HomeView::Tile::Tile (const juce::String& n, const juce::String& b, juce::Colour c) : name (n), blurb (b), colour (c)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void HomeView::Tile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    // A soft shadow, a graphite gradient, a tint of the kind's colour in the corner, a glow when hovered.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 12.0f);
    juce::ColourGradient grad (colours::card.brighter (hover ? 0.16f : 0.08f), r.getX(), r.getY(), colours::card.darker (0.12f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 12.0f);
    {
        juce::ColourGradient tint (colour.withAlpha (hover ? 0.22f : 0.12f), r.getX(), r.getY(), colour.withAlpha (0.0f), r.getX() + r.getWidth() * 0.6f, r.getY() + r.getHeight() * 0.8f, true);
        g.setGradientFill (tint);
        g.fillRoundedRectangle (r, 12.0f);
    }
    g.setColour (colour.withAlpha (hover ? 0.95f : 0.45f));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, hover ? 1.6f : 1.0f);
    g.setColour (colour);
    g.fillEllipse (r.getX() + 16.0f, r.getY() + 16.0f, 10.0f, 10.0f);
    if (hover)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (r.getX() + 11.0f, r.getY() + 11.0f, 20.0f, 20.0f);
    }
    g.setColour (colours::text);
    g.setFont (StacksLookAndFeel::font (22.0f, true));
    g.drawText (name, r.reduced (16.0f).withTrimmedTop (28.0f), juce::Justification::topLeft);
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (12.5f));
    g.drawFittedText (blurb, r.reduced (16.0f).withTrimmedTop (58.0f).toNearestInt(), juce::Justification::topLeft, 2);
}

std::unique_ptr<juce::AccessibilityHandler> HomeView::Tile::createAccessibilityHandler()
{
    return std::make_unique<PressHandler> (*this, "Make " + name + " sounds", [this] { if (onClick) onClick(); });
}

//==============================================================================
HomeView::HomeView (StacksAudioProcessor& p) : processor (p)
{
    for (const auto& k : kKinds)
    {
        auto tile = std::make_unique<Tile> (k.name, k.blurb, juce::Colour (k.colour));
        tile->onClick = [this, name = juce::String (k.name), t = tile.get()] { showKindMenu (name, *t); };
        addAndMakeVisible (*tile);
        tiles.push_back (std::move (tile));
    }
    growButton.setTooltip ("Five new sounds that grow out of the one you picked");
    growButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.85f));
    growButton.setColour (juce::TextButton::textColourOffId, colours::background);
    growButton.onClick = [this] { grow(); };
    addChildComponent (growButton);

    backButton.setTooltip ("Back to the kinds of sound");
    backButton.onClick = [this] { stage = Stage::pick; note.clear(); refresh(); };
    addChildComponent (backButton);

    saveButton.setTooltip ("Save the sound you are hearing: pick a folder and a name");
    saveButton.onClick = [this] { if (onSave) onSave(); };
    addChildComponent (saveButton);

    openSynthButton.setTooltip ("The full synth, for dialling this sound in by hand");
    openSynthButton.onClick = [this] { if (onOpenSynth) onOpenSynth(); };
    addChildComponent (openSynthButton);

    for (auto* b : { &closeButton, &mediumButton, &farButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        b->setColour (juce::TextButton::buttonOnColourId, colours::accentDim);
        addChildComponent (*b);
    }
    closeButton.setTooltip ("Small steps: siblings of the sound");
    mediumButton.setTooltip ("Clear changes, same family");
    farButton.setTooltip ("Big leaps: cousins rather than siblings");
    closeButton.onClick  = [this] { setVariation (0.3f); };
    mediumButton.onClick = [this] { setVariation (0.55f); };
    farButton.onClick    = [this] { setVariation (0.8f); };
    mediumButton.setToggleState (true, juce::dontSendNotification);

    processor.labBroadcaster.addChangeListener (this);
    refresh();
}

HomeView::~HomeView()
{
    processor.labBroadcaster.removeChangeListener (this);
    stopTimer();
}

void HomeView::setVariation (float v)
{
    variation = v;
}

juce::Colour HomeView::kindColour (const juce::String& kind) const
{
    for (const auto& k : kKinds) if (kind == k.name) return juce::Colour (k.colour);
    return colours::accent;
}

// Start fresh, or start from a preset of this kind and grow from it.
void HomeView::showKindMenu (const juce::String& kind, Tile& tile)
{
    if (processor.isBusy())
        return;
    juce::PopupMenu menu;
    menu.addItem (1, "Start fresh: five new " + kind.toLowerCase() + " sounds");
    juce::PopupMenu presets;
    juce::Array<juce::File> files;
    const auto root = StacksAudioProcessor::libraryRoot();
    juce::StringArray folders { kind };
    if (kind == "Texture") folders.add ("Drone");
    for (const auto& folder : folders)
    {
        files.addArray (root.getChildFile ("Factory").getChildFile (folder).findChildFiles (juce::File::findFiles, false, "*.json"));
        files.addArray (root.getChildFile ("My Sounds").findChildFiles (juce::File::findFiles, false, "*.json"));
    }
    files.sort();
    int id = 100;
    for (const auto& f : files)
    {
        if (id >= 100 + 40) break;
        presets.addItem (id++, f.getFileNameWithoutExtension());
    }
    if (files.isEmpty()) presets.addItem (99, "(no " + kind.toLowerCase() + " presets in the library yet)", false);
    menu.addSubMenu ("Start from a preset and grow it", presets);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tile).withMinimumWidth (260),
                        [this, kind, files] (int result)
                        {
                            if (result == 1) pick (kind);
                            else if (result >= 100 && result - 100 < files.size()) growFromPreset (files[result - 100]);
                        });
}

void HomeView::growFromPreset (const juce::File& file)
{
    if (! processor.loadLibraryPatch (file))
        return;
    note.clear();
    growthSeed = (int) (juce::Time::getMillisecondCounter() & 0x7fff) + 1;
    anim = 0.0f;
    falling.clear();
    seedName = processor.currentPatchName();
    seedStyle = GardenView::styleFor (processor.currentPatch(), processor.builtInWavetables(), true);
    stage = Stage::growing;
    processor.requestEvolve ("", variation);
    refresh();
}

void HomeView::pick (const juce::String& kind)
{
    if (processor.isBusy())
        return;
    category = kind;
    note.clear();
    growthSeed = (int) (juce::Time::getMillisecondCounter() & 0x7fff) + 1;
    anim = 0.0f;
    falling.clear();
    seedName = kind;
    seedStyle = GardenView::styleFor (processor.currentPatch(), processor.builtInWavetables(), true);
    for (const auto& k : kKinds)
        if (kind == k.name) seedStyle.colour = juce::Colour (k.colour);   // the slab wears the kind's colour
    stage = Stage::growing;
    processor.requestFromKind (kind);
    refresh();
}

void HomeView::grow()
{
    if (processor.isBusy() || processor.lab().auditioned < 0)
        return;
    note.clear();
    growthSeed = (int) (juce::Time::getMillisecondCounter() & 0x7fff) + 1;
    anim = 0.0f;
    falling.clear();
    seedName = processor.currentPatchName();
    seedStyle = GardenView::styleFor (processor.currentPatch(), processor.builtInWavetables(), true);
    stage = Stage::growing;
    processor.requestEvolve ("", variation);
    refresh();
}

void HomeView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void HomeView::refresh()
{
    const auto& lab = processor.lab();
    if (lab.generating)
        stage = Stage::growing;
    else if (stage == Stage::growing)
        stage = lab.candidates.empty() ? Stage::pick : Stage::results;
    else if (stage == Stage::results && lab.candidates.empty())
        stage = Stage::pick;

    for (auto& t : tiles) t->setVisible (stage == Stage::pick);
    if (stage == Stage::pick) falling.clear();
    else syncBlocks (stage == Stage::results && falling.empty());   // a restored batch sits; a live one falls in
    const bool results = stage == Stage::results;
    growButton.setVisible (results);
    growButton.setEnabled (results && lab.auditioned >= 0);
    saveButton.setVisible (results);
    saveButton.setEnabled (results && lab.auditioned >= 0);
    backButton.setVisible (stage != Stage::pick);
    backButton.setEnabled (stage != Stage::growing);
    openSynthButton.setVisible (results);
    for (auto* b : { &closeButton, &mediumButton, &farButton }) b->setVisible (results);

    placeBlocks();
    bool moving = stage == Stage::growing;
    for (const auto& f : falling) moving = moving || ! f.landed || f.scale < 0.995f;
    if (moving) { if (! isTimerRunning()) startTimerHz (30); }
    else stopTimer();
    resized();
    repaint();
}

juce::Rectangle<float> HomeView::stackArea() const
{
    auto r = getLocalBounds().reduced (24).toFloat();
    r.removeFromTop (54.0f + 30.0f + 26.0f);
    r.removeFromBottom (54.0f);
    return r;
}

juce::Rectangle<float> HomeView::faceOf (const Falling& f, juce::Rectangle<float> area) const
{
    const float bw = blockWidth (area), bh = blockHeight (area);
    const float w = bw * f.scale, hgt = bh * f.scale * f.squash;
    return { f.x + (bw - w) * 0.5f, f.y + bh - hgt, w, hgt };
}

void HomeView::syncBlocks (bool settled)
{
    const auto& lab = processor.lab();
    const auto area = stackArea();
    while (falling.size() > lab.candidates.size()) falling.pop_back();
    while (falling.size() < lab.candidates.size() && falling.size() < (size_t) StacksAudioProcessor::kAiPatchesPerBatch)
    {
        Falling f;
        f.index = (int) falling.size();
        const auto& patch = lab.candidates[(size_t) f.index];
        f.name = patch.name;
        f.style = GardenView::styleFor (patch, processor.builtInWavetables(), true);
        const auto slot = slotFor (f.index, area);
        f.x = slot.getX();
        if (settled) { f.y = slot.getY(); f.landed = true; f.scale = 1.0f; f.age = 10.0f; }
        else         { f.y = area.getY() - 160.0f; }
        falling.push_back (f);
    }
    for (int i = 0; i < (int) falling.size(); ++i)   // names and looks follow the batch (a tweak renames, a morph recolours)
    {
        const auto& patch = lab.candidates[(size_t) i];
        if (falling[(size_t) i].name != patch.name) { falling[(size_t) i].name = patch.name; falling[(size_t) i].style = GardenView::styleFor (patch, processor.builtInWavetables(), true); }
    }
}

void HomeView::stepPhysics (float dt)
{
    const auto area = stackArea();
    for (auto& f : falling)
    {
        f.age += dt;
        const auto slot = slotFor (f.index, area);
        f.x = slot.getX();
        const float target = slot.getY();
        if (! f.landed)
        {
            f.vy += 2600.0f * dt;                         // gravity
            f.y += f.vy * dt;
            f.scale += (1.06f - f.scale) * juce::jmin (1.0f, 7.0f * dt);   // grows in on the way down
            if (f.y >= target)
            {
                f.y = target;
                if (f.vy > 160.0f) { f.vy = -f.vy * 0.32f; f.squash = 0.82f; }   // bounce, squash
                else { f.vy = 0.0f; f.landed = true; f.squash = 0.9f; }
            }
        }
        else
        {
            f.y = target;
            f.scale += (1.0f - f.scale) * juce::jmin (1.0f, 6.0f * dt);
        }
        f.squash += (1.0f - f.squash) * juce::jmin (1.0f, 10.0f * dt);
    }
}

void HomeView::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    anim += dt;
    const auto& lab = processor.lab();
    syncBlocks (false);
    stepPhysics (dt);
    placeBlocks();
    const float p = lab.progress;
    const int done = (int) lab.candidates.size();
    const char* words = p < 0.0f ? "listening" : p < 0.3f ? "listening" : p < 0.6f ? "sketching" : p < 0.85f ? "shaping" : "tuning";
    stageText = lab.generating ? juce::String (words) + "   " + juce::String (done) + " of " + juce::String (StacksAudioProcessor::kAiPatchesPerBatch) : juce::String();
    if (! lab.generating)
    {
        bool moving = false;
        for (const auto& f : falling) moving = moving || ! f.landed || f.scale < 0.995f;
        if (! moving) stopTimer();
    }
    repaint();
}

void HomeView::BlockComp::paint (juce::Graphics& g)
{
    g.setOpacity (alpha);
    GardenView::paintBlock (g, face, style, name, selected, hovered, index);
}

std::unique_ptr<juce::AccessibilityHandler> HomeView::BlockComp::createAccessibilityHandler()
{
    return std::make_unique<PressHandler> (*this, "Hear " + name, [this] { owner.processor.audition (index); });
}

// Each block component sits where its physics says, with room for the top and side faces.
void HomeView::placeBlocks()
{
    const auto area = stackArea();
    while (blockComps.size() < falling.size())
    {
        auto b = std::make_unique<BlockComp> (*this);
        addAndMakeVisible (*b);
        blockComps.push_back (std::move (b));
    }
    while (blockComps.size() > falling.size()) blockComps.pop_back();
    const int selected = processor.lab().auditioned;
    for (size_t i = 0; i < falling.size(); ++i)
    {
        const auto& f = falling[i];
        auto& b = *blockComps[i];
        const auto face = faceOf (f, area);
        const float depth = f.style.depth + 2.0f;
        juce::Rectangle<float> bounds (face.getX() - 2.0f, face.getY() - depth, face.getWidth() + GardenView::kBlockSkew + 4.0f, face.getHeight() + depth + 2.0f);
        b.setBounds (bounds.toNearestInt());
        b.face = face.translated (-bounds.getX(), -bounds.getY()).translated ((float) bounds.toNearestInt().getX() - bounds.getX(), (float) bounds.toNearestInt().getY() - bounds.getY());
        b.index = f.index;
        b.style = f.style;
        if (b.name != f.name) { b.name = f.name; b.setTitle ("Hear " + f.name); b.setTooltip (f.name); b.invalidateAccessibilityHandler(); }
        b.alpha = juce::jmin (1.0f, f.age * 4.0f);
        b.selected = f.index == selected;
        b.setVisible (stage != Stage::pick);
        b.repaint();
    }
}

void HomeView::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (stage == Stage::pick ? 70 : 54);   // the heading, painted

    if (stage == Stage::pick)
    {
        const int cols = r.getWidth() > 900 ? 4 : 2, rows = ((int) tiles.size() + cols - 1) / cols;
        const int gap = 14;
        const int tileW = (r.getWidth() - gap * (cols - 1)) / cols;
        const int tileH = juce::jmin (230, (r.getHeight() - gap * (rows - 1)) / rows);
        for (int i = 0; i < (int) tiles.size(); ++i)
            tiles[(size_t) i]->setBounds (r.getX() + (i % cols) * (tileW + gap), r.getY() + (i / cols) * (tileH + gap), tileW, tileH);
        return;
    }

    auto top = r.removeFromTop (30);
    backButton.setBounds (top.removeFromLeft (110));
    if (stage == Stage::results)
    {
        auto bottom = r.removeFromBottom (40);
        openSynthButton.setBounds (bottom.removeFromRight (150));
        bottom.removeFromRight (10);
        saveButton.setBounds (bottom.removeFromRight (110));
        bottom.removeFromRight (16);
        farButton.setBounds (bottom.removeFromRight (70));
        mediumButton.setBounds (bottom.removeFromRight (80));
        closeButton.setBounds (bottom.removeFromRight (70));
        bottom.removeFromRight (16);
        growButton.setBounds (bottom.removeFromRight (190));
    }
    placeBlocks();
}

void HomeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (24);
    g.setColour (colours::text);
    if (stage == Stage::pick)
    {
        g.setFont (StacksLookAndFeel::font (30.0f, true));
        g.drawText ("What kind of sound?", r.removeFromTop (40), juce::Justification::centredLeft);
        g.setColour (colours::muted);
        g.setFont (StacksLookAndFeel::font (14.0f));
        g.drawText ("Pick one. You get five to play; grow the one you like.", r.removeFromTop (26), juce::Justification::centredLeft);
        return;
    }
    g.setFont (StacksLookAndFeel::font (24.0f, true));
    const auto& lab = processor.lab();
    const bool fromSeed = lab.seedIsPatch;
    const juce::String heading = stage == Stage::growing ? (fromSeed ? "Growing from \"" + lab.seed.name + "\"" : "Growing " + category.toLowerCase() + " sounds")
                                                         : (fromSeed ? "Grown from \"" + lab.seed.name + "\"" : "Five " + category.toLowerCase() + " sounds");
    g.drawText (heading, r.removeFromTop (34), juce::Justification::centredLeft);
    if (note.isNotEmpty())
    {
        g.setColour (colours::accent);
        g.setFont (StacksLookAndFeel::font (12.5f));
        g.drawText (note, r.removeFromTop (18), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (colours::muted);
        g.setFont (StacksLookAndFeel::font (12.5f));
        const juce::String line = lab.auditioned >= 0 && lab.auditioned < (int) lab.candidates.size()
            ? lab.candidates[(size_t) lab.auditioned].name + ":  " + lab.candidates[(size_t) lab.auditioned].description.upToFirstOccurrenceOf (" (", false, false)
            : juce::String ("Click a block to hear it, even while the rest are still growing. Play the keyboard. Then grow from it, save it, or open the synth.");
        g.drawText (line, r.removeFromTop (18), juce::Justification::centredLeft);
    }
    paintGrowth (g, stackArea());
}

// The slab is the seed; the five sounds fall onto it as they land, growing in
// on the way down and settling with a bounce. Empty slots wait as faint frames.
juce::Rectangle<float> HomeView::slotFor (int index, juce::Rectangle<float> area) const
{
    const float bw = blockWidth (area), bh = blockHeight (area), gap = 16.0f;
    const float rowW = 4.0f * bw + 3.0f * gap;
    const float x0 = area.getCentreX() - rowW * 0.5f;
    const float slabTop = area.getBottom() - 40.0f;
    if (index < 4)
        return { x0 + (float) index * (bw + gap), slabTop - bh - 6.0f, bw, bh };
    return { area.getCentreX() - bw * 0.5f, slabTop - 2.0f * bh - 12.0f, bw, bh };   // the fifth sits on top
}

void HomeView::paintGrowth (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int total = StacksAudioProcessor::kAiPatchesPerBatch;
    const float bw = blockWidth (area), bh = blockHeight (area);
    // The slab: the kind or the sound this batch grows from.
    const float rowW = 4.0f * bw + 3.0f * 16.0f;
    juce::Rectangle<float> slab (area.getCentreX() - rowW * 0.5f - 8.0f, area.getBottom() - 40.0f, rowW + 16.0f, 34.0f);
    auto slabStyle = seedStyle;
    slabStyle.depth = 10.0f;
    slabStyle.wave.clear();
    GardenView::paintBlock (g, slab, slabStyle, seedName, false, false, 0);

    // Waiting slots: faint frames that breathe; the one being written builds itself up.
    const auto& lab = processor.lab();
    for (int i = (int) falling.size(); i < total; ++i)
    {
        const auto slot = slotFor (i, area);
        if (i == (int) falling.size() && lab.generating)
        {
            paintConstruction (g, slot, seedStyle.colour, lab.progress);
            continue;
        }
        g.setColour (colours::muted.withAlpha (0.18f + 0.1f * std::sin (anim * 2.5f + (float) i)));
        g.drawRoundedRectangle (slot, 4.0f, 1.0f);
    }
    // The blocks, in flight or settled.
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (13.0f));
    if (stageText.isNotEmpty()) g.drawText (stageText, area.removeFromTop (22), juce::Justification::centred);
}

// The block being written assembles piece by piece: its edges trace in, the
// side and top faces slot on, the front fills, the wave draws itself, with
// sparks around the work. Driven by the engine's progress toward the next
// sound, or by time while it is still thinking.
void HomeView::paintConstruction (juce::Graphics& g, juce::Rectangle<float> slot, juce::Colour colour, float progress)
{
    // Real progress when the engine reports it; while it is still thinking, the
    // build loops on its own so the screen is never still.
    const float p = progress > 0.01f ? juce::jlimit (0.0f, 1.0f, progress) : std::fmod (anim * 0.16f, 1.0f);
    const float skew = GardenView::kBlockSkew, depth = 10.0f;
    auto stepIn = [] (float v, float a, float b) { return juce::jlimit (0.0f, 1.0f, (v - a) / (b - a)); };

    // 1. the front edges trace around (0 .. 0.35)
    {
        juce::Path outline;
        outline.addRoundedRectangle (slot, 4.0f);
        const float len = outline.getLength() * stepIn (p, 0.0f, 0.35f);
        juce::Path partial;
        for (float d = 0.0f; d < len; d += 6.0f)
        {
            const auto pt = outline.getPointAlongPath (d);
            if (d == 0.0f) partial.startNewSubPath (pt); else partial.lineTo (pt);
        }
        g.setColour (colour.withAlpha (0.9f));
        g.strokePath (partial, juce::PathStrokeType (1.6f));
        if (len > 0.0f && len < outline.getLength())
        {
            const auto tip = outline.getPointAlongPath (len);
            g.setColour (colours::text);
            g.fillEllipse (tip.x - 3.0f, tip.y - 3.0f, 6.0f, 6.0f);
        }
    }
    // 2. side face slides in from the right, top face drops in (0.3 .. 0.6)
    {
        const float t = stepIn (p, 0.3f, 0.6f);
        if (t > 0.0f)
        {
            juce::Path side;
            const float sx = slot.getRight() + skew * (2.0f - t);
            side.startNewSubPath (slot.getRight(), slot.getY());
            side.lineTo (sx, slot.getY() - depth);
            side.lineTo (sx, slot.getBottom() - depth);
            side.lineTo (slot.getRight(), slot.getBottom());
            side.closeSubPath();
            g.setColour (colour.darker (0.5f).withAlpha (t));
            g.fillPath (side);
            juce::Path top;
            const float ty = slot.getY() - depth * (2.0f - t);
            top.startNewSubPath (slot.getX(), slot.getY());
            top.lineTo (slot.getX() + skew, ty);
            top.lineTo (slot.getRight() + skew, ty);
            top.lineTo (slot.getRight(), slot.getY());
            top.closeSubPath();
            g.setColour (colour.brighter (0.25f).withAlpha (t));
            g.fillPath (top);
        }
    }
    // 3. the front fills bottom-up (0.5 .. 0.85)
    {
        const float t = stepIn (p, 0.5f, 0.85f);
        if (t > 0.0f)
        {
            auto fill = slot.withTrimmedTop (slot.getHeight() * (1.0f - t));
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (fill, 4.0f);
        }
    }
    // 4. the wave draws itself (0.7 .. 1.0)
    {
        const float t = stepIn (p, 0.7f, 1.0f);
        if (t > 0.0f)
        {
            juce::Path wave;
            const float x0 = slot.getX() + 5.0f, x1 = slot.getRight() - 5.0f, cy = slot.getY() + slot.getHeight() * 0.36f, amp = slot.getHeight() * 0.22f;
            const int steps = 48;
            for (int i = 0; i <= (int) (steps * t); ++i)
            {
                const float u = (float) i / (float) steps;
                const float x = x0 + (x1 - x0) * u;
                const float y = cy - amp * (0.7f * std::sin (u * 6.2831853f) + 0.3f * std::sin (u * 18.85f + anim));
                if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.strokePath (wave, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    // 5. sparks around the work
    juce::Random rng (growthSeed + (int) (anim * 6.0f));
    for (int i = 0; i < 6; ++i)
    {
        const float x = slot.getX() - 10.0f + rng.nextFloat() * (slot.getWidth() + 20.0f);
        const float y = slot.getY() - 16.0f + rng.nextFloat() * (slot.getHeight() + 24.0f);
        const float sz = 1.5f + rng.nextFloat() * 2.5f;
        g.setColour (colours::text.withAlpha (0.25f + 0.5f * rng.nextFloat()));
        g.fillEllipse (x, y, sz, sz);
    }
}

} // namespace stacks
