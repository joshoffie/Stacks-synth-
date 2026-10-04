#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>

#include "Parameters.h"

namespace stacks
{

// One cycle of an LFO shape as a lookup table, -1..1.
struct LfoTable
{
    static constexpr int kSize = 512;
    std::array<float, kSize + 1> v {};

    float at (float phase01) const noexcept
    {
        const float x = (phase01 - std::floor (phase01)) * (float) kSize;
        const int i = (int) x;
        const float t = x - (float) i;
        return v[(size_t) i] + t * (v[(size_t) i + 1] - v[(size_t) i]);
    }
};

// A drawn shape: points (x 0..1 ascending, y -1..1), joined by straight lines
// and wrapping from the last point back to the first.
struct LfoPoints
{
    std::vector<juce::Point<float>> points;

    static LfoPoints fromJson (const juce::String& json)
    {
        LfoPoints p;
        if (auto* arr = juce::JSON::parse (json).getArray())
            for (const auto& item : *arr)
                if (auto* xy = item.getArray(); xy != nullptr && xy->size() >= 2)
                    p.points.push_back ({ juce::jlimit (0.0f, 1.0f, (float) (double) (*xy)[0]), juce::jlimit (-1.0f, 1.0f, (float) (double) (*xy)[1]) });
        p.sort();
        return p;
    }

    juce::String toJson() const
    {
        juce::Array<juce::var> arr;
        for (const auto& pt : points)
            arr.add (juce::var (juce::Array<juce::var> { std::round (pt.x * 1000.0f) / 1000.0f, std::round (pt.y * 1000.0f) / 1000.0f }));
        return juce::JSON::toString (juce::var (arr), true);
    }

    void sort()
    {
        std::sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
    }

    float at (float x) const noexcept
    {
        if (points.empty()) return 0.0f;
        if (points.size() == 1) return points[0].y;
        x -= std::floor (x);
        // segment containing x, wrapping across 1.0
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto& a = points[i];
            const auto& b = points[(i + 1) % points.size()];
            float ax = a.x, bx = b.x;
            float xx = x;
            if (i + 1 == points.size()) { bx += 1.0f; if (xx < ax) xx += 1.0f; }
            if (xx >= ax && xx <= bx)
            {
                const float span = bx - ax;
                const float t = span > 1.0e-6f ? (xx - ax) / span : 0.0f;
                return a.y + t * (b.y - a.y);
            }
        }
        return points.front().y;
    }
};

inline LfoPoints defaultPointsForShape (int shape)
{
    LfoPoints p;
    switch (shape)
    {
        case ShapeTriangle: p.points = { { 0.0f, -1.0f }, { 0.5f, 1.0f } }; break;
        case ShapeSaw:      p.points = { { 0.0f, 1.0f }, { 0.999f, -1.0f } }; break;
        case ShapeRamp:     p.points = { { 0.0f, -1.0f }, { 0.999f, 1.0f } }; break;
        case ShapeSquare:   p.points = { { 0.0f, 1.0f }, { 0.499f, 1.0f }, { 0.5f, -1.0f }, { 0.999f, -1.0f } }; break;
        default: // sine, sampled
            for (int i = 0; i < 32; ++i)
            {
                const float x = (float) i / 32.0f;
                p.points.push_back ({ x, std::sin (juce::MathConstants<float>::twoPi * x) });
            }
            break;
    }
    return p;
}

inline void buildLfoTable (LfoTable& table, int shape, const LfoPoints& custom)
{
    for (int i = 0; i <= LfoTable::kSize; ++i)
    {
        const float x = (float) (i % LfoTable::kSize) / (float) LfoTable::kSize;
        float y;
        switch (shape)
        {
            case ShapeSine:     y = std::sin (juce::MathConstants<float>::twoPi * x); break;
            case ShapeTriangle: y = 1.0f - 4.0f * std::abs (x - 0.5f); break;
            case ShapeSaw:      y = 1.0f - 2.0f * x; break;
            case ShapeRamp:     y = 2.0f * x - 1.0f; break;
            case ShapeSquare:   y = x < 0.5f ? 1.0f : -1.0f; break;
            case ShapeCustom:   y = custom.points.empty() ? std::sin (juce::MathConstants<float>::twoPi * x) : custom.at (x); break;
            default:            y = 0.0f; break; // Random is handled by sample & hold
        }
        table.v[(size_t) i] = y;
    }
}

// Double-buffered tables the audio thread reads while the UI rebuilds.
struct LfoTableBank
{
    LfoTable storage[kNumLfos][2];
    std::atomic<int> active[kNumLfos] { 0, 0, 0, 0 };

    const LfoTable& get (int k) const noexcept { return storage[k][active[k].load (std::memory_order_acquire)]; }

    void set (int k, int shape, const LfoPoints& custom)
    {
        const int next = 1 - active[k].load();
        buildLfoTable (storage[k][next], shape, custom);
        active[k].store (next, std::memory_order_release);
    }
};

} // namespace stacks
