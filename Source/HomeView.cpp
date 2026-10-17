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
        juce::String getTitle() const override { return titleText; }
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
    g.setColour (hover ? colours::card.brighter (0.12f) : colours::card);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (colour.withAlpha (hover ? 0.9f : 0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, hover ? 2.0f : 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (r.getX() + 16.0f, r.getY() + 16.0f, 10.0f, 10.0f, 5.0f);
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
void HomeView::Card::set (const Patch& p, bool isSelected)
{
    name = p.name; category = p.category; selected = isSelected;
    description = p.description.upToFirstOccurrenceOf (" (", false, false);   // plain words only; the knob list stays in the synth
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    repaint();
}

void HomeView::Card::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (selected ? colours::accent.withAlpha (0.18f) : hover ? colours::card.brighter (0.1f) : colours::card);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (selected ? colours::accent : juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, selected ? 2.0f : 1.0f);
    // play mark
    juce::Path tri;
    const float cx = r.getX() + 26.0f, cy = r.getCentreY();
    tri.addTriangle (cx - 6.0f, cy - 8.0f, cx - 6.0f, cy + 8.0f, cx + 8.0f, cy);
    g.setColour (selected ? colours::accent : colours::muted);
    g.fillPath (tri);
    g.setColour (colours::text);
    g.setFont (StacksLookAndFeel::font (15.0f, true));
    g.drawText (name, r.withTrimmedLeft (50.0f).withTrimmedRight (12.0f).withHeight (r.getHeight() * 0.5f).withTrimmedTop (8.0f), juce::Justification::topLeft);
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (12.0f));
    g.drawFittedText (description, r.withTrimmedLeft (50.0f).withTrimmedRight (12.0f).withTrimmedTop (r.getHeight() * 0.5f).withTrimmedBottom (6.0f).toNearestInt(), juce::Justification::topLeft, 2);
}

std::unique_ptr<juce::AccessibilityHandler> HomeView::Card::createAccessibilityHandler()
{
    return std::make_unique<PressHandler> (*this, "Hear " + name, [this] { if (onClick) onClick(); });
}

//==============================================================================
HomeView::HomeView (StacksAudioProcessor& p) : processor (p)
{
    for (const auto& k : kKinds)
    {
        auto tile = std::make_unique<Tile> (k.name, k.blurb, juce::Colour (k.colour));
        tile->onClick = [this, name = juce::String (k.name)] { pick (name); };
        addAndMakeVisible (*tile);
        tiles.push_back (std::move (tile));
    }
    for (int i = 0; i < StacksAudioProcessor::kAiPatchesPerBatch; ++i)
    {
        auto card = std::make_unique<Card>();
        card->onClick = [this, i] { processor.audition (i); };
        addChildComponent (*card);
        cards.push_back (std::move (card));
    }

    growButton.setTooltip ("Five new sounds that grow out of the one you picked");
    growButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.85f));
    growButton.setColour (juce::TextButton::textColourOffId, colours::background);
    growButton.onClick = [this] { grow(); };
    addChildComponent (growButton);

    backButton.setTooltip ("Back to the kinds of sound");
    backButton.onClick = [this] { stage = Stage::pick; note.clear(); refresh(); };
    addChildComponent (backButton);

    saveButton.setTooltip ("Saves the sound you are hearing into My Sounds in the Library");
    saveButton.onClick = [this]
    {
        processor.savePreset (processor.currentPatchName(), StacksAudioProcessor::libraryRoot().getChildFile ("My Sounds"), true);
        note = "Saved \"" + processor.currentPatchName() + "\" to My Sounds";
        repaint();
    };
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
    for (int i = 0; i < (int) cards.size(); ++i)
    {
        const bool show = stage == Stage::results && i < (int) lab.candidates.size();
        cards[(size_t) i]->setVisible (show);
        if (show) cards[(size_t) i]->set (lab.candidates[(size_t) i], lab.auditioned == i);
    }
    const bool results = stage == Stage::results;
    growButton.setVisible (results);
    growButton.setEnabled (results && lab.auditioned >= 0);
    saveButton.setVisible (results);
    saveButton.setEnabled (results && lab.auditioned >= 0);
    backButton.setVisible (stage != Stage::pick);
    backButton.setEnabled (stage != Stage::growing);
    openSynthButton.setVisible (results);
    for (auto* b : { &closeButton, &mediumButton, &farButton }) b->setVisible (results);

    if (stage == Stage::growing) { if (! isTimerRunning()) startTimerHz (30); }
    else stopTimer();
    resized();
    repaint();
}

void HomeView::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    anim += dt;
    const auto& lab = processor.lab();

    // New sounds become blocks that fall onto the pile.
    auto area = getLocalBounds().reduced (24).toFloat().withTrimmedTop (54.0f + 30.0f + 40.0f);
    while (falling.size() < lab.candidates.size() && falling.size() < (size_t) StacksAudioProcessor::kAiPatchesPerBatch)
    {
        Falling f;
        f.index = (int) falling.size();
        const auto& patch = lab.candidates[(size_t) f.index];
        f.name = patch.name;
        f.style = GardenView::styleFor (patch, processor.builtInWavetables(), true);
        const auto slot = slotFor (f.index, area);
        f.x = slot.getX();
        f.y = area.getY() - 140.0f;
        f.vy = 0.0f;
        falling.push_back (f);
    }
    for (auto& f : falling)
    {
        f.age += dt;
        const auto slot = slotFor (f.index, area);
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
            f.scale += (1.0f - f.scale) * juce::jmin (1.0f, 6.0f * dt);
        f.squash += (1.0f - f.squash) * juce::jmin (1.0f, 10.0f * dt);
    }
    const float p = lab.progress;
    const int done = (int) lab.candidates.size();
    const char* words = p < 0.0f ? "listening" : p < 0.3f ? "listening" : p < 0.6f ? "sketching" : p < 0.85f ? "shaping" : "tuning";
    stageText = juce::String (words) + "   " + juce::String (done) + " of " + juce::String (StacksAudioProcessor::kAiPatchesPerBatch);
    repaint();
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
        saveButton.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (16);
        farButton.setBounds (bottom.removeFromRight (70));
        mediumButton.setBounds (bottom.removeFromRight (80));
        closeButton.setBounds (bottom.removeFromRight (70));
        bottom.removeFromRight (16);
        growButton.setBounds (bottom.removeFromRight (190));
        r.removeFromBottom (14);
        const int n = (int) cards.size();
        const int gap = 10;
        const int cardH = juce::jlimit (56, 96, (r.getHeight() - gap * (n - 1)) / n);
        int y = r.getY();
        for (auto& c : cards)
        {
            c->setBounds (r.getX(), y, r.getWidth(), cardH);
            y += cardH + gap;
        }
    }
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
    else if (stage == Stage::results)
    {
        g.setColour (colours::muted);
        g.setFont (StacksLookAndFeel::font (12.5f));
        g.drawText ("Click one to hear it. Play the keyboard. Then grow from it, save it, or open the synth to shape it.", r.removeFromTop (18), juce::Justification::centredLeft);
    }
    if (stage == Stage::growing)
        paintGrowth (g, r.withTrimmedTop (40).toFloat());
}

// The slab is the seed; the five sounds fall onto it as they land, growing in
// on the way down and settling with a bounce. Empty slots wait as faint frames.
juce::Rectangle<float> HomeView::slotFor (int index, juce::Rectangle<float> area) const
{
    const float bw = blockWidth (area), bh = blockHeight (area), gap = 14.0f;
    const float rowW = 4.0f * bw + 3.0f * gap;
    const float x0 = area.getCentreX() - rowW * 0.5f;
    const float slabTop = area.getBottom() - 46.0f;
    if (index < 4)
        return { x0 + (float) index * (bw + gap), slabTop - bh - 6.0f, bw, bh };
    return { area.getCentreX() - bw * 0.5f, slabTop - 2.0f * bh - 12.0f, bw, bh };   // the fifth sits on top
}

void HomeView::paintGrowth (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int total = StacksAudioProcessor::kAiPatchesPerBatch;
    const float bw = blockWidth (area), bh = blockHeight (area);
    // The slab: the kind or the sound this batch grows from.
    const float rowW = 4.0f * bw + 3.0f * 14.0f;
    juce::Rectangle<float> slab (area.getCentreX() - rowW * 0.5f - 8.0f, area.getBottom() - 46.0f, rowW + 16.0f, 34.0f);
    auto slabStyle = seedStyle;
    slabStyle.depth = 10.0f;
    slabStyle.wave.clear();
    GardenView::paintBlock (g, slab, slabStyle, seedName, false, false, 0);

    // Waiting slots: faint frames that breathe.
    for (int i = (int) falling.size(); i < total; ++i)
    {
        const auto slot = slotFor (i, area);
        g.setColour (colours::muted.withAlpha (0.18f + 0.1f * std::sin (anim * 2.5f + (float) i)));
        g.drawRoundedRectangle (slot, 4.0f, 1.0f);
    }
    // The blocks, in flight or settled.
    for (const auto& f : falling)
    {
        const float w = bw * f.scale, hgt = bh * f.scale * f.squash;
        juce::Rectangle<float> face (f.x + (bw - w) * 0.5f, f.y + bh - hgt, w, hgt);
        g.setOpacity (juce::jmin (1.0f, f.age * 4.0f));
        GardenView::paintBlock (g, face, f.style, f.name, f.landed, false, f.index);
        g.setOpacity (1.0f);
    }
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (13.0f));
    g.drawText (stageText, area.removeFromTop (22), juce::Justification::centred);
}

} // namespace stacks
