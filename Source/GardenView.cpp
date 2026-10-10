#include "GardenView.h"
#include "PatchGenerator.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    constexpr int kRowSizes[] = { 4, 3, 2, 1 };   // the pyramid, bottom row first (10 = one batch)
    constexpr float kGap = 10.0f, kSkew = 8.0f, kMaxDepth = 18.0f, kMargin = 16.0f;
    constexpr float kSlabH = 26.0f, kSlabDepth = 8.0f, kFooterH = 34.0f;
    constexpr double kDropMs = 420.0;

    juce::String heart() { return juce::String::fromUTF8 ("\xe2\x99\xa5"); }

    juce::Colour categoryColour (const juce::String& category)
    {
        const auto c = category.toLowerCase();
        if (c.contains ("pad"))     return juce::Colour (0xff6f8cff);
        if (c.contains ("bass"))    return juce::Colour (0xffd0583f);
        if (c.contains ("lead"))    return juce::Colour (0xffff8a3d);
        if (c.contains ("pluck"))   return juce::Colour (0xffc6d94a);
        if (c.contains ("key"))     return juce::Colour (0xff3fc9b3);
        if (c.contains ("bell"))    return juce::Colour (0xff62d6ff);
        if (c.contains ("texture")) return juce::Colour (0xff9a86c9);
        if (c.contains ("drone"))   return juce::Colour (0xff5c62b8);
        if (c.contains ("perc"))    return juce::Colour (0xffe0565a);
        return juce::Colour (0xff8d96a3);
    }

    // 0 at `lo`, 1 at `hi`, on a log scale.
    float logNorm (float v, float lo, float hi)
    {
        return juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (v, lo) / lo) / std::log (hi / lo));
    }

    // The three faces of a block whose front face is `f`.
    juce::Path topFace (juce::Rectangle<float> f, float depth)
    {
        juce::Path p;
        p.startNewSubPath (f.getX(), f.getY());
        p.lineTo (f.getX() + kSkew, f.getY() - depth);
        p.lineTo (f.getRight() + kSkew, f.getY() - depth);
        p.lineTo (f.getRight(), f.getY());
        p.closeSubPath();
        return p;
    }

    juce::Path sideFace (juce::Rectangle<float> f, float depth)
    {
        juce::Path p;
        p.startNewSubPath (f.getRight(), f.getY());
        p.lineTo (f.getRight() + kSkew, f.getY() - depth);
        p.lineTo (f.getRight() + kSkew, f.getBottom() - depth);
        p.lineTo (f.getRight(), f.getBottom());
        p.closeSubPath();
        return p;
    }
}

//==============================================================================
GardenView::Style GardenView::styleFor (const Patch& p, const WavetableBank& bank, bool ai)
{
    Style s;
    auto colour = categoryColour (p.category);
    if (! ai)
        colour = colour.withSaturation (colour.getSaturation() * 0.4f).withBrightness (colour.getBrightness() * 0.85f);
    // Brightness: a closed filter is a dark block, an open one lit.
    colour = colour.withMultipliedBrightness (0.72f + 0.4f * logNorm (p.get (P::filter_cutoff), 120.0f, 12000.0f));
    s.colour = colour;

    const int voices = (int) p.get (P::unison_voices);
    s.width = 0.9f + 0.25f * (voices > 1 ? p.get (P::unison_spread) : 0.0f);
    s.depth = 6.0f + (kMaxDepth - 6.0f) * logNorm (p.get (P::aenv_release), 0.03f, 4.0f);
    s.corner = 2.0f + 11.0f * logNorm (p.get (P::aenv_attack), 0.004f, 1.5f);
    s.serrated = p.get (P::dist_mix) > 0.15f;
    s.speckle = juce::jlimit (0.0f, 1.0f, p.get (P::noise_level) * 2.0f);
    s.glow = juce::jlimit (0.0f, 1.0f, p.get (P::reverb_mix));
    s.echo = juce::jlimit (0.0f, 1.0f, p.get (P::delay_mix));
    s.stripes = voices >= 3 ? juce::jmin (5, voices - 1) : 0;

    // An LFO on anything: the block breathes at that LFO's rate (capped so it stays calm).
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) p.get (modSourceParam (i));
        if (src >= SrcLfo1 && src <= SrcLfo4 && (int) p.get (modDestParam (i)) != TargetOff)
        {
            const int k = src - SrcLfo1;
            const bool synced = (int) p.get (lfoSyncParam (k)) != 0;
            s.bobHz = juce::jlimit (0.15f, 2.5f, synced ? 1.0f : p.get (lfoRateParam (k)));
            break;
        }
    }

    // One cycle of oscillator A at its morph position: the block wears its own wave.
    const int wave = (int) p.get (P::oscA_wave);
    const float morph = juce::jlimit (0.0f, 1.0f, p.get (P::oscA_morph));
    const int steps = 40;
    s.wave.resize ((size_t) steps + 1);
    float peak = 1.0e-3f;
    for (int i = 0; i <= steps; ++i)
    {
        const float phase = (float) i / (float) steps * 0.999f;
        float y;
        if (wave < WavetableBank::kNumBuiltIn)
            y = bank.read (wave, 0, morph, phase);
        else if (wave >= kCustomWave && ! p.waves[0].isEmpty())
        {
            // The designed table: its first spectrum, summed.
            y = 0.0f;
            const auto& frame = p.waves[0].frames.front();
            for (size_t h = 0; h < frame.size(); ++h)
                y += frame[h] * std::sin (juce::MathConstants<float>::twoPi * (float) (h + 1) * phase);
        }
        else
            y = std::sin (juce::MathConstants<float>::twoPi * phase);
        s.wave[(size_t) i] = y;
        peak = juce::jmax (peak, std::abs (y));
    }
    for (auto& y : s.wave) y /= peak;
    return s;
}

//==============================================================================
class GardenView::Block : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    Block (GardenView& g, int idx) : garden (g), index (idx) {}

    void set (const Patch& p, const Style& s, bool isAuditioned, bool isFavourite, int changesFromSeed)
    {
        name = p.name;
        style = s;
        ai = p.origin == "AI";
        auditioned = isAuditioned;
        favourite = isFavourite;
        setTooltip (p.name + (p.category.isNotEmpty() ? "  (" + p.category + ")" : "")
                    + (changesFromSeed >= 0 ? "  -  " + juce::String (changesFromSeed) + " audible changes from the seed" : juce::String()) + "\n" + p.description
                    + "\n\nclick: hear   drag up: exaggerate, drag down: blend toward the seed (live)   right-click: plant / save");
        setTitle ("Audition " + name);
        repaint();
    }

    // The front face, in this component's space.
    juce::Rectangle<float> face() const
    {
        return { kMargin, kMargin + kMaxDepth, (float) getWidth() - kMargin * 2.0f - kSkew, garden.blockHeight() };
    }

    bool hitTest (int x, int y) override
    {
        const auto f = face();
        if (f.contains ((float) x, (float) y)) return true;
        return topFace (f, style.depth).contains ((float) x, (float) y) || sideFace (f, style.depth).contains ((float) x, (float) y);
    }

    void paint (juce::Graphics& g) override
    {
        const auto f = face();
        const auto& s = style;
        const float alpha = juce::jlimit (0.0f, 1.0f, appear);
        if (alpha <= 0.0f) return;
        g.setOpacity (alpha);
        const auto colour = ai ? s.colour : s.colour.withAlpha (0.92f);

        // Reverb: a soft glow behind the block.
        if (s.glow > 0.02f)
        {
            auto halo = f.expanded (f.getWidth() * 0.25f * s.glow + 6.0f, f.getHeight() * 0.35f * s.glow + 6.0f);
            juce::ColourGradient grad (colour.withAlpha (0.28f * s.glow + (auditioned ? 0.12f : 0.0f)), halo.getCentreX(), halo.getCentreY(),
                                       colour.withAlpha (0.0f), halo.getX(), halo.getCentreY(), true);
            g.setGradientFill (grad);
            g.fillEllipse (halo);
        }

        // Delay: ghost outlines trailing off to the right.
        if (s.echo > 0.05f)
            for (int k = 1; k <= 2; ++k)
            {
                g.setColour (colour.withAlpha (0.3f * s.echo / (float) k));
                g.drawRoundedRectangle (f.translated (6.0f * (float) k, 4.0f * (float) k), s.corner, 1.2f);
            }

        // Side and top faces, then the front.
        g.setColour (colour.darker (0.5f));
        g.fillPath (sideFace (f, s.depth));
        g.setColour (auditioned ? colour.brighter (0.5f) : colour.brighter (0.25f));
        g.fillPath (topFace (f, s.depth));
        juce::ColourGradient front (colour.brighter (0.08f), f.getX(), f.getY(), colour.darker (0.12f), f.getX(), f.getBottom(), false);
        g.setGradientFill (front);
        g.fillRoundedRectangle (f, s.corner);

        // Distortion: a serrated top edge.
        if (s.serrated)
        {
            juce::Path teeth;
            const float tooth = 5.0f;
            for (float x = f.getX() + 2.0f; x + tooth <= f.getRight() - 2.0f; x += tooth)
                teeth.addTriangle (x, f.getY(), x + tooth, f.getY(), x + tooth * 0.5f, f.getY() + 3.5f);
            g.setColour (colour.darker (0.6f));
            g.fillPath (teeth);
        }

        // Unison: stripes.
        if (s.stripes > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            for (int i = 1; i <= s.stripes; ++i)
            {
                const float x = f.getX() + f.getWidth() * (float) i / (float) (s.stripes + 1);
                g.drawVerticalLine ((int) x, f.getY() + 3.0f, f.getBottom() - 3.0f);
            }
        }

        // Noise: speckle, the same dots every time for this block.
        if (s.speckle > 0.02f)
        {
            juce::Random rng ((juce::int64) index * 7919 + name.hashCode());
            const int dots = (int) (60.0f * s.speckle);
            for (int i = 0; i < dots; ++i)
            {
                const float x = f.getX() + 2.0f + rng.nextFloat() * (f.getWidth() - 4.0f);
                const float y = f.getY() + 2.0f + rng.nextFloat() * (f.getHeight() - 4.0f);
                g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.25f + 0.3f * s.speckle));
                g.fillEllipse (x, y, 1.6f, 1.6f);
            }
        }

        // The oscillator's wave across the upper half of the face.
        if (s.wave.size() > 2)
        {
            juce::Path wave;
            const float x0 = f.getX() + 5.0f, x1 = f.getRight() - 5.0f, cy = f.getY() + f.getHeight() * 0.36f, amp = f.getHeight() * 0.22f;
            for (size_t i = 0; i < s.wave.size(); ++i)
            {
                const float x = x0 + (x1 - x0) * (float) i / (float) (s.wave.size() - 1);
                const float y = cy - s.wave[i] * amp;
                if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            const bool lightFace = colour.getPerceivedBrightness() > 0.62f;
            g.setColour ((lightFace ? juce::Colours::black : juce::Colours::white).withAlpha (0.75f));
            g.strokePath (wave, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Name on the lower half.
        {
            const bool lightFace = colour.getPerceivedBrightness() > 0.62f;
            g.setColour (lightFace ? juce::Colours::black.withAlpha (0.85f) : juce::Colours::white.withAlpha (0.92f));
            g.setFont (StacksLookAndFeel::font (f.getHeight() > 64.0f ? 11.0f : 10.0f, true));
            g.drawFittedText (name, f.withTrimmedTop (f.getHeight() * 0.52f).reduced (3.0f, 2.0f).toNearestInt(), juce::Justification::centred, 2, 0.8f);
        }

        if (favourite)
        {
            g.setColour (colours::rowFilter);
            g.setFont (StacksLookAndFeel::font (11.0f, true));
            g.drawText (heart(), f.withTrimmedLeft (f.getWidth() - 16.0f).withHeight (14.0f).toNearestInt(), juce::Justification::centred, false);
        }

        if (auditioned || hovered)
        {
            g.setColour (auditioned ? colours::text : colours::text.withAlpha (0.5f));
            g.drawRoundedRectangle (f.expanded (0.5f), s.corner, auditioned ? 1.8f : 1.0f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true; garden.hoveredBlock = index; garden.layoutBlocks(); garden.repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hovered = false; if (garden.hoveredBlock == index) garden.hoveredBlock = -1; garden.layoutBlocks(); garden.repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            juce::Component::SafePointer<Block> safe (this);   // the block may be gone by the time the menu is used
            menu.addItem ("Plant: evolve from this", [safe] { if (safe != nullptr && safe->garden.onEvolveFrom) safe->garden.onEvolveFrom (safe->index); });
            menu.addItem ("Save...", [safe]
            {
                if (safe == nullptr) return;
                safe->garden.processor.audition (safe->index);
                if (safe->garden.onSave) safe->garden.onSave();
            });
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
            return;
        }
        garden.processor.audition (index);
        home = getPosition();
        dragging = false;
        morphT = 1.0f;
        toFront (false);
    }

    // Drag up: the sound is exaggerated past the candidate; drag down: it blends
    // back toward the seed. The sound follows live, the block comes back to its
    // place on release.
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int dy = juce::jlimit (-90, 90, e.getDistanceFromDragStartY());
        if (! dragging && std::abs (dy) < 4) return;
        dragging = true;
        morphT = juce::jlimit (0.05f, 1.6f, 1.0f - (float) dy / 130.0f);
        setTopLeftPosition (home.x, home.y + dy);
        garden.repaint();

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastMorphMs > 40.0)
        {
            lastMorphMs = now;
            garden.processor.morphCandidate (index, morphT);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        garden.processor.commitMorph (index, morphT);
        garden.layoutBlocks();
        garden.repaint();
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { garden.processor.audition (index); }));
    }

    GardenView& garden;
    int index;
    juce::String name;
    Style style;
    bool ai = false, auditioned = false, favourite = false, hovered = false, dragging = false;
    float appear = 1.0f;          // 0..1 while dropping in
    float morphT = 1.0f;
    juce::Point<int> home;
    double lastMorphMs = 0.0;
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

float GardenView::baseBlockWidth() const
{
    const float avail = (float) getWidth() - 24.0f - kSkew;
    return juce::jlimit (36.0f, 80.0f, (avail - 3.0f * kGap) / 4.0f);
}

float GardenView::blockHeight() const
{
    // Four rows plus the slab, the footer and some sky have to fit; never wider than tall by much.
    const float room = ((float) getHeight() - kFooterH - kSlabH - kMaxDepth - 40.0f) / 4.0f;
    return juce::jlimit (48.0f, baseBlockWidth() * 0.95f, room);
}

int GardenView::rowOf (int index, int& column, int& count) const
{
    int row = 0, first = 0;
    for (;; ++row)
    {
        count = row < 4 ? kRowSizes[row] : 1;   // past one batch: keep stacking single blocks
        if (index < first + count) { column = index - first; return row; }
        first += count;
    }
}

juce::Rectangle<float> GardenView::slabFace() const
{
    const float w = 4.0f * baseBlockWidth() + 3.0f * kGap + 12.0f;
    const float y = (float) getHeight() - kFooterH - 8.0f - kSlabH;
    return { ((float) getWidth() - kSkew - w) * 0.5f, y, w, kSlabH };
}

juce::Rectangle<float> GardenView::slotFor (int index) const
{
    int column = 0, count = 1;
    const int row = rowOf (index, column, count);
    const float bw = baseBlockWidth();
    // Blocks in a row keep their own widths; the row stays centred.
    std::vector<float> widths;
    float total = 0.0f;
    const int first = index - column;
    for (int j = 0; j < count; ++j)
    {
        const int i = first + j;
        float w = bw;
        if (i < (int) blocks.size()) w = bw * blocks[(size_t) i]->style.width;
        widths.push_back (w);
        total += w;
    }
    total += (float) (count - 1) * kGap;
    const float roomW = (float) getWidth() - 24.0f - kSkew;
    const float scale = total > roomW ? roomW / total : 1.0f;
    float x = ((float) getWidth() - kSkew - total * scale) * 0.5f;
    for (int j = 0; j < column; ++j) x += (widths[(size_t) j] + kGap) * scale;
    const float y = slabFace().getY() - (float) (row + 1) * blockHeight();
    return { x, y, widths[(size_t) column] * scale, blockHeight() };
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

    while (blocks.size() < lab.candidates.size())
    {
        auto block = std::make_unique<Block> (*this, (int) blocks.size());
        addAndMakeVisible (*block);
        blocks.push_back (std::move (block));
    }
    while (blocks.size() > lab.candidates.size())
        blocks.pop_back();

    const auto& bank = processor.builtInWavetables();
    for (int i = 0; i < (int) blocks.size(); ++i)
    {
        const auto& c = lab.candidates[(size_t) i];
        blocks[(size_t) i]->set (c, styleFor (c, bank, c.origin == "AI"), i == lab.auditioned, c.favourite,
                                 lab.seedIsPatch ? countAudibleDifferences (c, lab.seed) : -1);
    }
    seedStyled = lab.seedIsPatch;
    if (seedStyled) seedStyle = styleFor (lab.seed, bank, true);

    layoutBlocks();
    updateTimer();
    repaint();
}

void GardenView::updateTimer()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    bool dropping = processor.lab().generating;
    for (double t : appearedAt) if (now - t < kDropMs) dropping = true;
    bool bobbing = false;
    if (! processor.calmMode())
        for (const auto& b : blocks) if (b->style.bobHz > 0.0f) bobbing = true;
    if (! isVisible()) { stopTimer(); return; }
    if (dropping)       startTimerHz (30);
    else if (bobbing)   startTimerHz (20);
    else                stopTimer();
}

void GardenView::visibilityChanged()
{
    updateTimer();
}

void GardenView::timerCallback()
{
    layoutBlocks();
    repaint();
    updateTimer();
}

// Every block to its slot in the pile, dropping in when new, lifted when
// hovered or playing, bobbing when an LFO moves its sound.
void GardenView::layoutBlocks()
{
    const auto& lab = processor.lab();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const int shown = (int) juce::jmin (blocks.size(), lab.candidates.size(), appearedAt.size());
    for (int i = 0; i < shown; ++i)
    {
        auto& block = *blocks[(size_t) i];
        const auto slot = slotFor (i);
        const float drop = (float) juce::jlimit (0.0, 1.0, (now - appearedAt[(size_t) i]) / kDropMs);
        const float eased = 1.0f - (1.0f - drop) * (1.0f - drop) * (1.0f - drop);
        block.appear = juce::jmin (1.0f, drop * 2.5f);
        float lift = (i == lab.auditioned ? 4.0f : 0.0f) + (block.hovered ? 2.0f : 0.0f);
        if (block.style.bobHz > 0.0f && ! processor.calmMode())
            lift += 1.5f + 1.5f * std::sin ((float) (now * 0.001 * block.style.bobHz) * juce::MathConstants<float>::twoPi + (float) i * 1.7f);
        const float y = slot.getY() - lift - (1.0f - eased) * 70.0f;
        const int w = (int) (slot.getWidth() + kSkew + 2.0f * kMargin), h = (int) (blockHeight() + kMaxDepth + 2.0f * kMargin);
        if (! block.dragging)
            block.setBounds ((int) std::round (slot.getX() - kMargin), (int) std::round (y - kMargin - kMaxDepth), w, h);
        else
            block.setSize (w, h);
    }
}

void GardenView::resized()
{
    layoutBlocks();
}

void GardenView::mouseDown (const juce::MouseEvent& e)
{
    if (slabFace().expanded (0.0f, kSlabDepth).contains (e.position))
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem ("Generate from the description (new generation from scratch)", [this] { if (onFresh) onFresh(); });
            menu.addItem ("Evolve this sound", [this] { if (onEvolve) onEvolve(); });
            menu.addItem ("Save this sound...", [this] { if (onSave) onSave(); });
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
    const bool generating = lab.generating;
    const auto slab = slabFace();
    // The seed is what this generation grew from, not whatever is loaded now.
    const auto seedLabel = lab.generation > 0 && lab.seed.name.isNotEmpty() ? lab.seed.name : processor.currentPatchName();

    // The slab: the seed, styled like its sound when it is one, plain when it is a prompt.
    {
        const auto colour = seedStyled ? seedStyle.colour : colours::card.brighter (0.15f);
        const float pulse = generating ? 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.004f) : 0.0f;
        if (generating || (seedStyled && seedStyle.glow > 0.05f))
        {
            auto halo = slab.expanded (14.0f + 10.0f * pulse, 10.0f + 6.0f * pulse);
            juce::ColourGradient grad ((generating ? colours::accent : colour).withAlpha (0.12f + 0.2f * pulse + 0.2f * (seedStyled ? seedStyle.glow : 0.0f)), halo.getCentreX(), halo.getCentreY(),
                                       colours::accent.withAlpha (0.0f), halo.getX(), halo.getCentreY(), true);
            g.setGradientFill (grad);
            g.fillEllipse (halo);
        }
        g.setColour (colour.darker (0.5f));
        g.fillPath (sideFace (slab, kSlabDepth));
        g.setColour (colour.brighter (0.25f));
        g.fillPath (topFace (slab, kSlabDepth));
        g.setColour (colour);
        g.fillRoundedRectangle (slab, 3.0f);
        g.setColour (seedStyled ? colours::text.withAlpha (0.35f) : colours::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (slab.expanded (0.5f), 3.0f, 1.0f);

        const bool lightFace = colour.getPerceivedBrightness() > 0.62f;
        g.setColour (lightFace ? juce::Colours::black.withAlpha (0.85f) : colours::text);
        g.setFont (StacksLookAndFeel::font (11.0f, true));
        g.drawFittedText (seedLabel, slab.reduced (8.0f, 2.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }

    // Empty slots while a batch is still coming: where the next ideas land.
    if (generating)
    {
        g.setColour (colours::text.withAlpha (0.12f));
        for (int i = (int) lab.candidates.size(); i < StacksAudioProcessor::kBatchSize; ++i)
        {
            const float dashes[] = { 4.0f, 4.0f };
            juce::Path outline, dashed;
            outline.addRoundedRectangle (slotFor (i).reduced (1.0f), 4.0f);
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.fillPath (dashed);
        }
    }

    // A drag in progress: how far the sound is being pushed.
    for (const auto& b : blocks)
        if (b->dragging)
        {
            const auto face = b->getBounds().toFloat().reduced (kMargin).withTrimmedTop (kMaxDepth);
            const auto text = b->morphT < 1.0f ? "blend " + juce::String (juce::roundToInt (b->morphT * 100.0f)) + "%"
                                               : "x" + juce::String (b->morphT, 2);
            auto pill = juce::Rectangle<float> (64.0f, 18.0f).withCentre ({ face.getCentreX(), face.getY() - 16.0f });
            g.setColour (colours::background.withAlpha (0.85f));
            g.fillRoundedRectangle (pill, 9.0f);
            g.setColour (colours::accent);
            g.setFont (StacksLookAndFeel::font (10.5f, true));
            g.drawText (text, pill.toNearestInt(), juce::Justification::centred, false);
        }

    // The hovered block's description, or what the model is doing; nothing otherwise.
    auto footer = getLocalBounds().removeFromBottom ((int) kFooterH).reduced (6, 2);
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (10.5f));
    juce::String text;
    if (hoveredBlock >= 0 && hoveredBlock < (int) lab.candidates.size())
        text = lab.candidates[(size_t) hoveredBlock].description;
    else if (generating)
        text = lab.progressDetail.isNotEmpty() ? lab.progressDetail : "designing...";
    else if (lab.candidates.empty())
        text = "Describe a sound and press Generate";
    if (text.isNotEmpty())
        g.drawFittedText (text, footer, juce::Justification::centred, 2, 0.9f);
}

} // namespace stacks
