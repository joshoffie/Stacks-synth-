#include "Skin.h"
#include <juce_core/juce_core.h>

namespace stacks::skin
{

namespace
{
    struct State
    {
        std::map<juce::String, juce::Image> images;
        std::map<juce::String, std::array<float, 4>> slices;   // logical px: left, top, right, bottom
        std::map<juce::String, juce::Image> tinted;             // "name#aarrggbb" -> coloured copy
        float scale = 2.0f;
        bool knobArc = true;
    };

    State& state()
    {
        static State s;
        return s;
    }

    void readConfig (const juce::File& folder)
    {
        const auto parsed = juce::JSON::parse (folder.getChildFile ("skin.json"));
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return;
        auto& s = state();
        if (obj->hasProperty ("scale"))   s.scale = juce::jlimit (1.0f, 4.0f, (float) obj->getProperty ("scale"));
        if (obj->hasProperty ("knobArc")) s.knobArc = (bool) obj->getProperty ("knobArc");
        if (auto* sl = obj->getProperty ("slices").getDynamicObject())
            for (const auto& prop : sl->getProperties())
                if (auto* arr = prop.value.getArray(); arr != nullptr && arr->size() == 4)
                    s.slices[prop.name.toString()] = { (float) (*arr)[0], (float) (*arr)[1], (float) (*arr)[2], (float) (*arr)[3] };
    }
}

juce::File userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Stacks Patches").getChildFile ("skin");
}

void loadFrom (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;
    auto& s = state();
    for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*.png"))
    {
        auto image = juce::ImageFileFormat::loadFrom (file);
        if (image.isValid())
            s.images[file.getFileNameWithoutExtension()] = image;
    }
    readConfig (folder);
    s.tinted.clear();
}

void load()
{
    auto& s = state();
    s = State();
    const auto bundle = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getParentDirectory();
    loadFrom (bundle.getChildFile ("Resources").getChildFile ("Skin"));
    loadFrom (userFolder());
    const auto env = juce::SystemStats::getEnvironmentVariable ("STACKS_SKIN_DIR", {});
    if (env.isNotEmpty())
        loadFrom (juce::File (env));
}

bool any()                                   { return ! state().images.empty(); }
bool has (const juce::String& name)          { return state().images.count (name) > 0; }
float scale()                                { return state().scale; }
bool knobArc()                               { return state().knobArc; }

const juce::Image* image (const juce::String& name)
{
    const auto& images = state().images;
    const auto it = images.find (name);
    return it == images.end() ? nullptr : &it->second;
}

void drawStretched (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, float opacity)
{
    if (auto* img = image (name))
    {
        g.setOpacity (opacity);
        g.drawImage (*img, area, juce::RectanglePlacement::stretchToFit, false);
        g.setOpacity (1.0f);
    }
}

void drawCentred (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, float opacity)
{
    if (auto* img = image (name))
    {
        const float w = (float) img->getWidth() / scale(), h = (float) img->getHeight() / scale();
        g.setOpacity (opacity);
        g.drawImage (*img, juce::Rectangle<float> (w, h).withCentre (area.getCentre()), juce::RectanglePlacement::stretchToFit, false);
        g.setOpacity (1.0f);
    }
}

void drawNine (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, float fallbackMargin, float opacity)
{
    auto* img = image (name);
    if (img == nullptr)
        return;
    const auto& s = state();
    std::array<float, 4> m { fallbackMargin, fallbackMargin, fallbackMargin, fallbackMargin };
    if (const auto it = s.slices.find (name); it != s.slices.end())
        m = it->second;
    const float k = s.scale;
    const float iw = (float) img->getWidth(), ih = (float) img->getHeight();
    // margins can't exceed what the image or the area has room for
    const float l = juce::jmin (m[0], iw / k * 0.5f, area.getWidth() * 0.5f), r = juce::jmin (m[2], iw / k * 0.5f, area.getWidth() * 0.5f);
    const float t = juce::jmin (m[1], ih / k * 0.5f, area.getHeight() * 0.5f), b = juce::jmin (m[3], ih / k * 0.5f, area.getHeight() * 0.5f);
    const float sx[4] = { 0.0f, l * k, iw - r * k, iw };
    const float sy[4] = { 0.0f, t * k, ih - b * k, ih };
    const float dx[4] = { area.getX(), area.getX() + l, area.getRight() - r, area.getRight() };
    const float dy[4] = { area.getY(), area.getY() + t, area.getBottom() - b, area.getBottom() };
    g.setOpacity (opacity);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
        {
            const float sw = sx[i + 1] - sx[i], sh = sy[j + 1] - sy[j];
            const float dw = dx[i + 1] - dx[i], dh = dy[j + 1] - dy[j];
            if (sw < 0.5f || sh < 0.5f || dw < 0.01f || dh < 0.01f) continue;
            g.drawImage (*img, (int) std::round (dx[i]), (int) std::round (dy[j]), (int) std::round (dw), (int) std::round (dh),
                         (int) std::round (sx[i]), (int) std::round (sy[j]), (int) std::round (sw), (int) std::round (sh), false);
        }
    g.setOpacity (1.0f);
}

void drawFilmstrip (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, float position, float opacity)
{
    auto* img = image (name);
    if (img == nullptr || img->getWidth() <= 0)
        return;
    const int frameW = img->getWidth();
    const int frames = juce::jmax (1, img->getHeight() / frameW);
    const int frame = juce::jlimit (0, frames - 1, (int) std::round (juce::jlimit (0.0f, 1.0f, position) * (float) (frames - 1)));
    const auto src = juce::Rectangle<int> (0, frame * frameW, frameW, frameW);
    // keep the frame square and centred in the area
    const float side = juce::jmin (area.getWidth(), area.getHeight());
    const auto dest = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
    g.setOpacity (opacity);
    g.drawImage (img->getClippedImage (src), dest, juce::RectanglePlacement::stretchToFit, false);
    g.setOpacity (1.0f);
}

void drawTinted (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour tint, float opacity)
{
    auto* img = image (name);
    if (img == nullptr)
        return;
    auto& cache = state().tinted;
    const auto key = name + "#" + tint.toString();
    auto it = cache.find (key);
    if (it == cache.end())
    {
        juce::Image copy = img->createCopy();
        juce::Image::BitmapData data (copy, juce::Image::BitmapData::readWrite);
        const float tr = tint.getFloatRed(), tg = tint.getFloatGreen(), tb = tint.getFloatBlue();
        for (int y = 0; y < data.height; ++y)
            for (int x = 0; x < data.width; ++x)
            {
                const auto c = data.getPixelColour (x, y);
                data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.getFloatRed() * tr, c.getFloatGreen() * tg, c.getFloatBlue() * tb, c.getFloatAlpha()));
            }
        if (cache.size() > 256) cache.clear();
        it = cache.emplace (key, std::move (copy)).first;
    }
    g.setOpacity (opacity);
    g.drawImage (it->second, area, juce::RectanglePlacement::stretchToFit, false);
    g.setOpacity (1.0f);
}

} // namespace stacks::skin
