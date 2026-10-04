#include "StacksLookAndFeel.h"
#include "Controls.h"

namespace stacks
{

namespace
{
    const juce::Colour kOutline   { 0xff3a3f49 };
    const juce::Colour kWidget    { 0xff2d313a };
    const juce::Colour kWidgetHi  { 0xff383d48 };
    const juce::Colour kCapLight  { 0xff4a505c };
    const juce::Colour kCapDark   { 0xff1e2127 };
    const juce::Colour kTrack     { 0xff2a2e36 };

    // The first of these that is installed. Avenir Next ships with macOS and
    // reads like a product, not a dev tool; the rest are fallbacks.
    juce::String pickFontName()
    {
        static const juce::String chosen = []
        {
            const auto installed = juce::Font::findAllTypefaceNames();
            for (auto* candidate : { "Avenir Next", "SF Pro Display", "Helvetica Neue", "Inter", "Futura" })
                if (installed.contains (candidate))
                    return juce::String (candidate);
            return juce::Font::getDefaultSansSerifFontName();
        }();
        return chosen;
    }
}

StacksLookAndFeel::StacksLookAndFeel()
{
    auto scheme = getMidnightColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, colours::background);
    scheme.setUIColour (ColourScheme::widgetBackground, colours::panel);
    scheme.setUIColour (ColourScheme::menuBackground, colours::panel);
    scheme.setUIColour (ColourScheme::defaultFill, colours::accent);
    scheme.setUIColour (ColourScheme::highlightedFill, colours::accent);
    scheme.setUIColour (ColourScheme::defaultText, colours::text);
    scheme.setUIColour (ColourScheme::highlightedText, juce::Colours::black);
    scheme.setUIColour (ColourScheme::outline, kOutline);
    setColourScheme (scheme);

    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, kTrack);
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, kTrack);
    setColour (juce::TextButton::buttonColourId, kWidget);
    setColour (juce::TextButton::buttonOnColourId, colours::accentDim);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, kWidget);
    setColour (juce::ComboBox::outlineColourId, kOutline);
    setColour (juce::ComboBox::arrowColourId, colours::muted);
    setColour (juce::TextEditor::backgroundColourId, colours::background);
    setColour (juce::TextEditor::outlineColourId, kOutline);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha (0.7f));
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accentDim);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::muted);
    setColour (juce::ScrollBar::thumbColourId, kCapLight);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff343943));
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, kOutline);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, kOutline);
}

juce::String StacksLookAndFeel::displayFontName() { return pickFontName(); }

juce::Font StacksLookAndFeel::font (float height, bool isBold)
{
    return juce::Font (juce::FontOptions (pickFontName(), height, isBold ? juce::Font::bold : juce::Font::plain));
}

juce::Typeface::Ptr StacksLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // Every font in the UI resolves to the display typeface, bold or regular.
    auto& cache = f.isBold() ? bold : regular;
    if (cache == nullptr)
        cache = juce::Typeface::createSystemTypefaceFor (juce::Font (juce::FontOptions (pickFontName(), 14.0f, f.isBold() ? juce::Font::bold : juce::Font::plain)));
    return cache != nullptr ? cache : LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font StacksLookAndFeel::getLabelFont (juce::Label& l)              { return l.getFont(); }
juce::Font StacksLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, (float) h * 0.55f), true); }
juce::Font StacksLookAndFeel::getComboBoxFont (juce::ComboBox& box)       { return font (juce::jmin (13.0f, (float) box.getHeight() * 0.6f)); }
juce::Font StacksLookAndFeel::getPopupMenuFont()                          { return font (13.0f); }
juce::Font StacksLookAndFeel::getAlertWindowMessageFont()                 { return font (14.0f); }
juce::Font StacksLookAndFeel::getAlertWindowTitleFont()                   { return font (17.0f, true); }

//==============================================================================
void StacksLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    // Same geometry as LookAndFeel_V4 (reduced by 10, arc just inside the edge)
    // so the modulation rings drawn around the knob keep lining up.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcW = juce::jlimit (2.5f, 4.0f, radius * 0.16f);
    const float arcRadius = radius - arcW * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = slider.isEnabled();

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with a soft glow
    if (sliderPos > 0.002f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (fill.withAlpha (enabled ? 0.25f : 0.1f));
        g.strokePath (value, juce::PathStrokeType (arcW + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (enabled ? fill : fill.withAlpha (0.4f));
        g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cap: shadow, shaded body, rim highlight
    const float capRadius = juce::jmax (4.0f, arcRadius - arcW * 0.5f - 4.0f);
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (cap.translated (0.0f, 1.5f).expanded (0.5f));
    juce::ColourGradient body (kCapLight, centre.x - capRadius * 0.6f, centre.y - capRadius * 0.7f,
                               kCapDark,  centre.x + capRadius * 0.6f, centre.y + capRadius * 0.9f, true);
    g.setGradientFill (body);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (cap.reduced (0.6f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (cap, 0.8f);

    // Pointer
    const float pointerLen = capRadius * 0.55f, pointerW = juce::jlimit (1.6f, 2.6f, capRadius * 0.14f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerW * 0.5f, -capRadius + 2.5f, pointerW, pointerLen, pointerW * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (enabled ? colours::text : colours::muted);
    g.fillPath (pointer);
}

void StacksLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const float trackW = 4.0f;
    juce::Rectangle<float> track = horizontal ? juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - trackW * 0.5f, (float) width, trackW)
                                              : juce::Rectangle<float> ((float) x + (float) width * 0.5f - trackW * 0.5f, (float) y, trackW, (float) height);
    g.setColour (kTrack);
    g.fillRoundedRectangle (track, trackW * 0.5f);

    juce::Rectangle<float> filled = horizontal ? track.withRight (sliderPos) : track.withTop (sliderPos);
    if (! horizontal) filled = track.withTop (sliderPos);
    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (filled, trackW * 0.5f);

    const float thumbR = 6.0f;
    const auto thumb = horizontal ? juce::Point<float> (sliderPos, track.getCentreY()) : juce::Point<float> (track.getCentreX(), sliderPos);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (thumb.x - thumbR, thumb.y - thumbR + 1.0f, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (thumb.x - thumbR, thumb.y - thumbR, thumbR * 2.0f, thumbR * 2.0f);
}

void StacksLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = juce::jmin (6.0f, r.getHeight() * 0.3f);
    auto base = backgroundColour;
    if (button.getToggleState()) base = button.findColour (juce::TextButton::buttonOnColourId);
    if (down)                    base = base.brighter (0.15f);
    else if (highlighted)        base = base.brighter (0.08f);

    juce::ColourGradient grad (base.brighter (0.06f), r.getX(), r.getY(), base.darker (0.08f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
    g.setColour (button.getToggleState() ? colours::accent.withAlpha (0.6f) : juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void StacksLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const float boxSize = 14.0f;
    auto box = juce::Rectangle<float> (2.0f, ((float) button.getHeight() - boxSize) * 0.5f, boxSize, boxSize);
    g.setColour (kTrack.brighter (highlighted ? 0.1f : 0.0f));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (kOutline.brighter (0.2f));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (button.getToggleState())
    {
        g.setColour (button.findColour (juce::ToggleButton::tickColourId));
        g.fillRoundedRectangle (box.reduced (3.5f), 1.5f);
    }
    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (font (11.5f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft ((int) boxSize + 8), juce::Justification::centredLeft, 1);
}

void StacksLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // chevron
    const float cx = (float) width - 11.0f, cy = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.3f));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void StacksLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void StacksLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 5.0f);
}

void StacksLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true) ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                                                : editor.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 5.0f, 1.0f);
}

void StacksLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (findColour (juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 6.0f);
    g.setColour (kOutline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 6.0f, 1.0f);
}

juce::Label* StacksLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (11.5f));
    label->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    return label;
}

} // namespace stacks
