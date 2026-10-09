#include "ModulatorsPanel.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    constexpr int kRowH = 20;

    int sourceForTab (int tab)
    {
        switch (tab)
        {
            case 0: return SrcLfo1;
            case 1: return SrcLfo2;
            case 2: return SrcLfo3;
            case 3: return SrcLfo4;
            case 4: return SrcModEnv;
            default: return SrcVelocity;
        }
    }

    const char* tabName (int tab)
    {
        static const char* names[] = { "LFO 1", "LFO 2", "LFO 3", "LFO 4", "MOD ENV", "MORE" };
        return names[juce::jlimit (0, 5, tab)];
    }
}

//==============================================================================
LfoDisplay::LfoDisplay (StacksAudioProcessor& p, int lfoIndex, juce::Colour c)
    : processor (p), lfo (lfoIndex), colour (c)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setTooltip ("Click to add a point, drag to move it, double-click to remove. Drawing switches the shape to Custom.");
    startTimerHz (30);
}

LfoDisplay::~LfoDisplay()
{
    stopTimer();
}

juce::Point<float> LfoDisplay::toNorm (juce::Point<float> s) const
{
    const auto r = getLocalBounds().toFloat().reduced (4.0f);
    return { juce::jlimit (0.0f, 1.0f, (s.x - r.getX()) / r.getWidth()),
             juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (s.y - r.getY()) / r.getHeight()) };
}

juce::Point<float> LfoDisplay::toScreen (juce::Point<float> n) const
{
    const auto r = getLocalBounds().toFloat().reduced (4.0f);
    return { r.getX() + n.x * r.getWidth(), r.getY() + (1.0f - n.y) * 0.5f * r.getHeight() };
}

int LfoDisplay::hitPoint (juce::Point<float> s) const
{
    for (int i = 0; i < (int) points.points.size(); ++i)
        if (toScreen (points.points[(size_t) i]).getDistanceFrom (s) < 9.0f)
            return i;
    return -1;
}

void LfoDisplay::beginEditing()
{
    points = processor.lfoPoints (lfo);
    const int shape = (int) processor.apvts.getRawParameterValue (paramId (lfoShapeParam (lfo)))->load();
    if (points.points.empty() || shape != ShapeCustom)
        points = defaultPointsForShape (shape == ShapeRandom ? ShapeSine : shape);
}

void LfoDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem ("Reset to the selected shape", [this] { processor.setLfoPoints (lfo, {}); });
        menu.showMenuAsync (juce::PopupMenu::Options());
        return;
    }

    beginEditing();
    const auto pos = e.position;
    dragIndex = hitPoint (pos);
    if (dragIndex < 0)
    {
        points.points.push_back (toNorm (pos));
        points.sort();
        dragIndex = hitPoint (pos);
    }
    processor.setLfoPoints (lfo, points);
}

void LfoDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0 || dragIndex >= (int) points.points.size())
        return;

    auto n = toNorm (e.position);
    auto& pts = points.points;
    // keep x between the neighbours so the order never flips
    const float lo = dragIndex > 0 ? pts[(size_t) dragIndex - 1].x + 0.002f : 0.0f;
    const float hi = dragIndex + 1 < (int) pts.size() ? pts[(size_t) dragIndex + 1].x - 0.002f : 1.0f;
    n.x = juce::jlimit (lo, juce::jmax (lo, hi), n.x);
    pts[(size_t) dragIndex] = n;
    processor.setLfoPoints (lfo, points);
}

void LfoDisplay::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
}

void LfoDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    beginEditing();
    const int hit = hitPoint (e.position);
    if (hit >= 0 && points.points.size() > 2)
    {
        points.points.erase (points.points.begin() + hit);
        processor.setLfoPoints (lfo, points);
    }
}

void LfoDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    StacksLookAndFeel::drawInset (g, r);

    // grid
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    const auto inner = r.reduced (4.0f);
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (inner.getX() + inner.getWidth() * (float) i / 4.0f), inner.getY(), inner.getBottom());
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());

    // shape
    const auto& table = processor.lfoTable (lfo);
    const int shape = (int) processor.apvts.getRawParameterValue (paramId (lfoShapeParam (lfo)))->load();
    juce::Path path;
    const int steps = 128;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const float y = shape == ShapeRandom ? 0.0f : table.at (x);
        const auto s = toScreen ({ x, y });
        if (i == 0) path.startNewSubPath (s); else path.lineTo (s);
    }
    juce::Path fill (path);
    fill.lineTo (toScreen ({ 1.0f, 0.0f }));
    fill.lineTo (toScreen ({ 0.0f, 0.0f }));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (shape == ShapeRandom)
    {
        g.setColour (colours::muted);
        g.setFont (StacksLookAndFeel::font (10.5f));
        g.drawText ("new value every cycle", inner, juce::Justification::centred);
    }

    // points when custom
    if (shape == ShapeCustom)
    {
        const auto pts = processor.lfoPoints (lfo);
        for (const auto& p : pts.points)
        {
            const auto s = toScreen (p);
            g.setColour (colours::text);
            g.fillEllipse (s.x - 3.0f, s.y - 3.0f, 6.0f, 6.0f);
        }
    }

    // playhead
    if (processor.calmMode()) return;
    const float phase = processor.lfoDisplayPhase (lfo) + processor.apvts.getRawParameterValue (paramId (lfoPhaseParam (lfo)))->load();
    const float px = inner.getX() + (phase - std::floor (phase)) * inner.getWidth();
    g.setColour (colours::text.withAlpha (0.45f));
    g.drawVerticalLine ((int) px, inner.getY(), inner.getBottom());
}

//==============================================================================
ModulatorsPanel::ModulatorsPanel (StacksAudioProcessor& p) : processor (p)
{
    for (int t = 0; t < kNumTabs; ++t)
    {
        auto& b = tabButtons[t];
        b.setButtonText (tabName (t));
        b.source = sourceForTab (t);
        b.setTooltip (t == TabSources ? "Velocity, key, wheel, aftertouch, random" : "Click to edit. Drag onto any knob to connect it.");
        styleAsTab (b);
        b.setColour (juce::TextButton::buttonOnColourId, modSourceColour (sourceForTab (t)));
        b.onClick = [this, t] { showTab (t); };
        addAndMakeVisible (b);
    }

    for (int k = 0; k < kNumMacros; ++k)
        sourcePicker.addItem (macroNames()[k] + " (macro)", SrcMacro1 + k);
    sourcePicker.addItem ("Velocity", SrcVelocity);
    sourcePicker.addItem ("Key", SrcKey);
    sourcePicker.addItem ("Mod Wheel", SrcModWheel);
    sourcePicker.addItem ("Aftertouch", SrcAftertouch);
    sourcePicker.addItem ("Slide (MPE, CC74)", SrcSlide);
    sourcePicker.addItem ("Random (per note)", SrcRandom);
    sourcePicker.addItem ("Filter Env", SrcFilterEnv);
    sourcePicker.setSelectedId (SrcVelocity, juce::dontSendNotification);
    sourcePicker.onChange = [this] { refreshConnections(); };
    addChildComponent (sourcePicker);

    assignButton.setButtonText ("Assign");
    assignButton.setTooltip ("Then click any knob to modulate it (click here again to cancel)");
    assignButton.onClick = [this]
    {
        if (assigningSource >= 0) { if (onAssignCancel) onAssignCancel(); }
        else if (onAssignRequest) onAssignRequest (currentSource());
    };
    addAndMakeVisible (assignButton);

    virtualButton.setButtonText ("Pitch / Amp / Pan");
    virtualButton.setTooltip ("Targets that aren't knobs");
    virtualButton.onClick = [this] { addVirtualTargetMenu(); };
    addAndMakeVisible (virtualButton);

    emptyLabel.setText ("no connections", juce::dontSendNotification);
    emptyLabel.setFont (StacksLookAndFeel::font (10.5f));
    emptyLabel.setColour (juce::Label::textColourId, colours::muted.withAlpha (0.7f));
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (emptyLabel);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);

    processor.labBroadcaster.addChangeListener (this);
    showTab (TabLfo1);
}

ModulatorsPanel::~ModulatorsPanel()
{
    processor.labBroadcaster.removeChangeListener (this);
}

int ModulatorsPanel::currentSource() const
{
    return currentTab == TabSources ? sourcePicker.getSelectedId() : sourceForTab (currentTab);
}

void ModulatorsPanel::setAssigning (int source)
{
    assigningSource = source;
    const bool on = source >= 0;
    assignButton.setButtonText (on ? "Click a knob..." : "Assign");
    assignButton.setColour (juce::TextButton::buttonColourId, on ? modSourceColour (source) : juce::Colour (0xff23272d));
    assignButton.setColour (juce::TextButton::textColourOffId, on ? juce::Colours::black : colours::text);
}

void ModulatorsPanel::showTab (int tab)
{
    currentTab = tab;
    for (int t = 0; t < kNumTabs; ++t)
        tabButtons[t].setToggleState (t == tab, juce::dontSendNotification);

    content.clear();
    display.reset();
    sourcePicker.setVisible (tab == TabSources);

    const auto colour = modSourceColour (currentSource());
    auto& apvts = processor.apvts;

    if (tab <= TabLfo4)
    {
        const int k = tab;
        display = std::make_unique<LfoDisplay> (processor, k, colour);
        addAndMakeVisible (*display);

        auto add = [&] (P param, bool knob)
        {
            const auto& spec = stacks::spec (param);
            std::unique_ptr<juce::Component> c;
            if (knob) { auto kn = std::make_unique<ParamKnob> (apvts, spec); kn->setAccent (colour); c = std::move (kn); }
            else      { c = std::make_unique<ParamChoice> (apvts, spec); }
            addAndMakeVisible (*c);
            content.push_back (std::move (c));
        };
        add (lfoShapeParam (k), false);
        add (lfoSyncParam (k), false);
        add (lfoModeParam (k), false);
        add (lfoRateParam (k), true);
        add (lfoPhaseParam (k), true);
    }
    else if (tab == TabModEnv)
    {
        for (auto param : { P::menv_attack, P::menv_decay, P::menv_sustain, P::menv_release })
        {
            auto kn = std::make_unique<ParamKnob> (apvts, stacks::spec (param));
            kn->setAccent (colour);
            addAndMakeVisible (*kn);
            content.push_back (std::move (kn));
        }
    }

    refreshConnections();
    refreshEnabled();
    resized();
}

void ModulatorsPanel::addVirtualTargetMenu()
{
    juce::PopupMenu menu;
    const int source = currentSource();
    menu.addItem ("Pitch (vibrato, pitch drops)", [this, source] { processor.addModulation (source, TargetPitch, 0.05f); });
    menu.addItem ("Pitch B (osc B only)",         [this, source] { processor.addModulation (source, TargetPitchB, 0.1f); });
    menu.addItem ("Amp (tremolo, velocity)",       [this, source] { processor.addModulation (source, TargetAmp, 0.5f); });
    menu.addItem ("Pan",                           [this, source] { processor.addModulation (source, TargetPan, 0.5f); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (virtualButton));
}

void ModulatorsPanel::refreshConnections()
{
    rows.clear();
    const int source = currentSource();
    for (int slot : processor.modulationsFor (source))
    {
        auto row = std::make_unique<Row>();
        const int target = (int) processor.apvts.getRawParameterValue (paramId (modDestParam (slot)))->load();
        row->name.setText (modTargetNames()[juce::jlimit (0, modTargetNames().size() - 1, target)], juce::dontSendNotification);
        row->name.setFont (StacksLookAndFeel::font (11.0f));
        row->name.setColour (juce::Label::textColourId, colours::text);
        row->name.setInterceptsMouseClicks (false, false);
        row->depth.setSliderStyle (juce::Slider::LinearHorizontal);
        row->depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row->depth.setPopupDisplayEnabled (true, false, this);
        row->depth.setColour (juce::Slider::trackColourId, modSourceColour (source));
        row->depth.setColour (juce::Slider::thumbColourId, colours::text);
        row->depth.setTooltip ("Depth: drag left/right (negative inverts)");
        row->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId (modAmountParam (slot)), row->depth);
        row->remove.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));   // ×
        row->remove.setTooltip ("Remove this connection");
        styleAsTab (row->remove);
        row->remove.onClick = [this, slot] { processor.clearModulation (slot); };
        list.addAndMakeVisible (row->name);
        list.addAndMakeVisible (row->depth);
        list.addAndMakeVisible (row->remove);
        rows.push_back (std::move (row));
    }

    for (int t = 0; t < kNumTabs; ++t)
    {
        const int count = (int) processor.modulationsFor (sourceForTab (t)).size();
        tabButtons[t].setButtonText (count > 0 && t != TabSources ? juce::String (tabName (t)) + " " + juce::String (count) : juce::String (tabName (t)));
    }

    resized();
}

void ModulatorsPanel::showSource (int source)
{
    if (source >= SrcLfo1 && source <= SrcLfo4)      showTab (TabLfo1 + (source - SrcLfo1));
    else if (source == SrcModEnv)                    showTab (TabModEnv);
    else
    {
        sourcePicker.setSelectedId (source, juce::dontSendNotification);
        showTab (TabSources);
    }
}

void ModulatorsPanel::refreshEnabled()
{
    if (currentTab > TabLfo4 || content.size() < 4)
        return;
    const bool synced = (int) processor.apvts.getRawParameterValue (paramId (lfoSyncParam (currentTab)))->load() != 0;
    if (content[3]->isEnabled() == synced)
        content[3]->setEnabled (! synced);   // content[3] is the Rate knob
}

void ModulatorsPanel::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void ModulatorsPanel::resized()
{
    auto r = getLocalBounds();
    auto tabs = r.removeFromTop (20);
    const int tabW = juce::jmin (74, tabs.getWidth() / kNumTabs);
    for (auto& b : tabButtons)
        b.setBounds (tabs.removeFromLeft (tabW));
    r.removeFromTop (4);

    auto right = r.removeFromRight (juce::jmin (236, r.getWidth() / 3));
    r.removeFromRight (8);
    auto body = r;

    const int cell = 62, cellH = juce::jmin (70, body.getHeight());
    if (currentTab <= TabLfo4)
    {
        auto knobsArea = body.removeFromRight (2 * cell);
        body.removeFromRight (4);
        auto combos = body.removeFromRight (92);
        body.removeFromRight (6);
        if (display) display->setBounds (body);
        const int comboH = juce::jmin (36, combos.getHeight() / 3);
        for (int i = 0; i < 3 && i < (int) content.size(); ++i)
            content[(size_t) i]->setBounds (combos.removeFromTop (comboH));
        auto knobRow = knobsArea.withSizeKeepingCentre (knobsArea.getWidth(), cellH);
        for (int i = 3; i < (int) content.size(); ++i)
            content[(size_t) i]->setBounds (knobRow.removeFromLeft (cell));
    }
    else if (currentTab == TabModEnv)
    {
        auto knobRow = body.withSizeKeepingCentre (body.getWidth(), cellH);
        for (auto& c : content)
            c->setBounds (knobRow.removeFromLeft (cell));
    }
    else
    {
        sourcePicker.setBounds (body.removeFromTop (22).removeFromLeft (180));
    }

    auto buttons = right.removeFromTop (22);
    assignButton.setBounds (buttons.removeFromLeft (juce::jmin (96, buttons.getWidth() / 2)));
    buttons.removeFromLeft (4);
    virtualButton.setBounds (buttons);
    right.removeFromTop (4);
    viewport.setBounds (right);
    emptyLabel.setBounds (right);
    emptyLabel.setVisible (rows.empty());

    const int width = viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? viewport.getScrollBarThickness() : 0);
    list.setSize (juce::jmax (1, width), (int) rows.size() * kRowH);
    int y = 0;
    for (auto& row : rows)
    {
        auto rr = juce::Rectangle<int> (0, y, width, kRowH);
        row->remove.setBounds (rr.removeFromRight (22));
        row->name.setBounds (rr.removeFromLeft (juce::jmin (96, rr.getWidth() / 2)));
        row->depth.setBounds (rr);
        y += kRowH;
    }
}

} // namespace stacks
