#include "StacksLookAndFeel.h"
#include "Controls.h"

namespace stacks
{

namespace
{
    const juce::Colour kWidget   { 0xff23272d };
    const juce::Colour kWidgetHi { 0xff2b3037 };
    const juce::Colour kTrack    { 0xff2a2f36 };
    const juce::Colour kCapLight { 0xff3b414a };
    const juce::Colour kCapDark  { 0xff1a1d22 };

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

    bool isTab (const juce::Button& b) { return (bool) b.getProperties()["tab"]; }
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
    scheme.setUIColour (ColourScheme::outline, colours::outline);
    setColourScheme (scheme);

    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, kTrack);
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, kTrack);
    setColour (juce::TextButton::buttonColourId, kWidget);
    setColour (juce::TextButton::buttonOnColourId, colours::accentDim);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, kWidget);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::arrowColourId, colours::muted);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::background);
    setColour (juce::TextEditor::outlineColourId, colours::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha (0.8f));
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accentDim);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::muted);
    setColour (juce::ToggleButton::textColourId, colours::muted);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff3a4048));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff2a2f36));
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::outline);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::outline);
    setColour (juce::Label::textColourId, colours::text);
}

juce::String StacksLookAndFeel::displayFontName() { return pickFontName(); }

juce::Font StacksLookAndFeel::font (float height, bool isBold)
{
    return juce::Font (juce::FontOptions (pickFontName(), height, isBold ? juce::Font::bold : juce::Font::plain));
}

void StacksLookAndFeel::drawCard (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (colours::card);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void StacksLookAndFeel::drawInset (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
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
juce::Font StacksLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    return font (juce::jmin (isTab (b) ? 11.0f : 12.0f, (float) h * 0.55f), true).withExtraKerningFactor (isTab (b) ? 0.06f : 0.0f);
}
juce::Font StacksLookAndFeel::getComboBoxFont (juce::ComboBox& box)       { return font (juce::jmin (11.5f, (float) box.getHeight() * 0.6f)); }
juce::Font StacksLookAndFeel::getPopupMenuFont()                          { return font (12.5f); }
juce::Font StacksLookAndFeel::getAlertWindowMessageFont()                 { return font (13.5f); }
juce::Font StacksLookAndFeel::getAlertWindowTitleFont()                   { return font (16.0f, true); }

//==============================================================================
void StacksLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (kKnobMargin);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 4.0f)
        return;
    const auto centre = bounds.getCentre();
    const float arcW = juce::jlimit (2.0f, 5.0f, radius * 0.13f);
    const float arcRadius = radius - arcW * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = slider.isEnabled();
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroPos = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
    const float zeroAngle = startAngle + zeroPos * (endAngle - startAngle);

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc, from zero for bipolar knobs, with a soft glow
    if (std::abs (angle - zeroAngle) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        g.setColour (fill.withAlpha (enabled ? 0.22f : 0.08f));
        g.strokePath (value, juce::PathStrokeType (arcW + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (enabled ? fill : fill.withAlpha (0.4f));
        g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cap: a flat disc with a faint top light and a dark rim
    const float capRadius = juce::jmax (3.0f, arcRadius - arcW * 0.5f - 3.0f);
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (cap.translated (0.0f, 1.0f).expanded (0.6f));
    juce::ColourGradient body (kCapLight, centre.x - capRadius * 0.5f, centre.y - capRadius * 0.8f,
                               kCapDark,  centre.x + capRadius * 0.4f, centre.y + capRadius * 0.9f, true);
    g.setGradientFill (body);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (cap.reduced (0.8f), 1.0f);

    // Pointer: a coloured line from the rim toward the centre
    const float pointerLen = capRadius * 0.52f, pointerW = juce::jlimit (1.6f, 4.0f, capRadius * 0.13f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerW * 0.5f, -capRadius + 2.0f, pointerW, pointerLen, pointerW * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (enabled ? (fill.getBrightness() > 0.3f ? fill.brighter (0.25f) : colours::text) : colours::muted);
    g.fillPath (pointer);
}

void StacksLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const float trackW = 3.0f;
    juce::Rectangle<float> track = horizontal ? juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - trackW * 0.5f, (float) width, trackW)
                                              : juce::Rectangle<float> ((float) x + (float) width * 0.5f - trackW * 0.5f, (float) y, trackW, (float) height);
    g.setColour (kTrack);
    g.fillRoundedRectangle (track, trackW * 0.5f);

    // Bipolar sliders fill from the middle.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zero = horizontal ? (bipolar ? (float) x + (float) width * (float) slider.valueToProportionOfLength (0.0) : (float) x)
                                  : (bipolar ? (float) y + (float) height * (1.0f - (float) slider.valueToProportionOfLength (0.0)) : (float) (y + height));
    juce::Rectangle<float> filled = horizontal ? juce::Rectangle<float> (juce::jmin (zero, sliderPos), track.getY(), std::abs (sliderPos - zero), trackW)
                                               : juce::Rectangle<float> (track.getX(), juce::jmin (zero, sliderPos), trackW, std::abs (sliderPos - zero));
    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (filled, trackW * 0.5f);

    const float thumbR = 5.0f;
    const auto thumb = horizontal ? juce::Point<float> (sliderPos, track.getCentreY()) : juce::Point<float> (track.getCentreX(), sliderPos);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (thumb.x - thumbR, thumb.y - thumbR + 1.0f, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (thumb.x - thumbR, thumb.y - thumbR, thumbR * 2.0f, thumbR * 2.0f);
}

void StacksLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat();
    if (isTab (button))
    {
        // A tab: no box, an underline in its colour when selected.
        if (button.getToggleState())
        {
            auto onColour = button.findColour (juce::TextButton::buttonOnColourId);
            g.setColour (onColour.getBrightness() < 0.35f ? colours::accent : onColour);
            g.fillRoundedRectangle (r.removeFromBottom (2.0f).reduced (6.0f, 0.0f), 1.0f);
        }
        else if (highlighted || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.06f : 0.035f));
            g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
        }
        return;
    }

    r = r.reduced (0.5f);
    const float corner = juce::jmin (5.0f, r.getHeight() * 0.3f);
    auto base = backgroundColour;
    const bool on = button.getToggleState();
    if (on)              base = button.findColour (juce::TextButton::buttonOnColourId);
    if (down)            base = base.brighter (0.12f);
    else if (highlighted) base = base.brighter (0.06f);

    g.setColour (base);
    g.fillRoundedRectangle (r, corner);
    g.setColour (on ? colours::accent.withAlpha (0.55f) : juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void StacksLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    const auto f = getTextButtonFont (button, button.getHeight());
    g.setFont (f);
    juce::Colour c = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    if (isTab (button))
    {
        const auto onColour = button.findColour (juce::TextButton::buttonOnColourId);
        const bool bright = (bool) button.getProperties()["bright"];   // header icons read as controls, not as dim tabs
        c = button.getToggleState() ? (onColour.getBrightness() < 0.35f ? colours::accent : onColour)
                                    : highlighted || bright ? colours::text : colours::muted;
    }
    g.setColour (c.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    const int margin = juce::jmin (6, button.proportionOfWidth (0.1f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (margin, 1), juce::Justification::centred, 1, 0.8f);
}

void StacksLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const float boxSize = 13.0f;
    auto box = juce::Rectangle<float> (2.0f, ((float) button.getHeight() - boxSize) * 0.5f, boxSize, boxSize);
    g.setColour (kTrack.brighter (highlighted ? 0.1f : 0.0f));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (colours::outline.brighter (0.2f));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (button.getToggleState())
    {
        g.setColour (button.findColour (juce::ToggleButton::tickColourId));
        g.fillRoundedRectangle (box.reduced (3.5f), 1.5f);
    }
    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (font (11.0f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft ((int) boxSize + 7), juce::Justification::centredLeft, 1);
}

void StacksLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // chevron
    const float cx = (float) width - 9.0f, cy = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 3.0f, cy - 1.5f);
    chevron.lineTo (cx, cy + 1.5f);
    chevron.lineTo (cx + 3.0f, cy - 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.3f));
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void StacksLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (5, 1, box.getWidth() - 20, box.getHeight() - 2);
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
    g.setColour (colours::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 6.0f, 1.0f);
}

void StacksLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height, bool vertical,
                                       int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) width - 4.0f, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) height - 4.0f);
    g.setColour (bar.findColour (juce::ScrollBar::thumbColourId).withAlpha (over || down ? 1.0f : 0.7f));
    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

juce::Label* StacksLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (11.0f));
    label->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    return label;
}

} // namespace stacks
