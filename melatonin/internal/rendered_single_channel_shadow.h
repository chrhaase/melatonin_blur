#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
#include <melatonin_blur/tests/helpers/pixel_helpers.h>

namespace melatonin
{
    // these are the parameters required to represent a single drop or inner shadow
    // wish I could put these in shadows.h to help people
    struct ShadowParameters
    {
        // one single color per shadow
        juce::Colour color = {};
        int radius = 1;
        juce::Point<int> offset = { 0, 0 };

        // Spread literally just expands or contracts the *path* size
        // Inverted for inner shadows
        int spread = 0;

        // an inner shadow is just a modified drop shadow
        bool inner = false;
    };

    namespace internal
    {
        // encapsulates logic for rendering a path to inner/drop shadow
        // the image is optimized to be as small as possible
        // the path is always 0,0 in the image
        class RenderedSingleChannelShadow
        {
        public:
            ShadowParameters parameters;

            explicit RenderedSingleChannelShadow (ShadowParameters p) : parameters (p) {}

            juce::Image& render (juce::Path& originAgnosticPath, float scale, bool stroked = false);

            // Offset is added on the fly, it's not actually a part of the render
            // and can change without invalidating cache
            juce::Rectangle<int> getScaledBounds()
            {
                return scaledShadowBounds + scaledOffset;
            }

            juce::Rectangle<int> getScaledPathBounds()
            {
                return scaledPathBounds;
            }

            [[nodiscard]]

            const juce::Image&
                getImage()
            {
                return singleChannelRender;
            }

            [[nodiscard]] bool updateRadius (int radius)
            {
                if (juce::approximatelyEqual (radius, parameters.radius))
                    return false;

                parameters.radius = radius;
                return true;
            }

            [[nodiscard]] bool updateSpread (int spread)
            {
                if (juce::approximatelyEqual (spread, parameters.spread))
                    return false;

                parameters.spread = spread;
                return true;
            }

            [[nodiscard]] bool updateOffset (juce::Point<int> offset, float scale)
            {
                if (offset == parameters.offset)
                    return false;

                parameters.offset = offset;
                scaledOffset = (parameters.offset * scale).roundToInt();
                return true;
            }

            [[nodiscard]] bool updateColor (juce::Colour color)
            {
                if (color == parameters.color)
                    return false;

                parameters.color = color;
                return true;
            }

            [[nodiscard]] bool updateOpacity (float opacity)
            {
                if (juce::approximatelyEqual (opacity, parameters.color.getFloatAlpha()))
                    return false;

                parameters.color = parameters.color.withAlpha (opacity);
                return true;
            }

            // this doesn't re-render, just re-calculates position stuff
            void updateScaledShadowBounds (float scale)
            {
                // By default, match the main graphics context's scaling factor.
                // This lets us render retina / high quality shadows.
                // We can only use an integer numbers for blurring (hence the rounding)
                scaledSpread = juce::roundToInt ((float) parameters.spread * scale);
                scaledRadius = juce::roundToInt ((float) parameters.radius * scale);
                scaledOffset = (parameters.offset * scale).roundToInt();

                // account for our scaled radius and spread
                // one might think that inner shadows don't need to expand with radius
                // since they are clipped to path bounds
                // however, when there's offset, and we are making position-agnostic shadows!
                if (parameters.inner)
                {
                    scaledShadowBounds = scaledPathBounds.expanded (scaledRadius - scaledSpread, scaledRadius - scaledSpread);
                }
                else
                    scaledShadowBounds = scaledPathBounds.expanded (scaledRadius + scaledSpread, scaledRadius + scaledSpread);

                // TODO: Investigate/test if this is ever relevant / how to apply to position agnostic
                // I'm guessing reduces the clip size in the edge case it doesn't overlap the main context?
                // It comes from JUCE's shadow classes
                //.getIntersection (g.getClipBounds().expanded (s.radius + s.spread + 1));

                // if the scale isn't an integer, we'll be dealing with subpixel compositing
                // for example a 4.5px image centered in a canvas technically has the width of 6 pixels
                // (the outer 2 pixels will be 25%-ish opacity)
                // this is a problem because we're going to be blurring the image
                // and don't want to cut our blurs off early
                if (!juce::approximatelyEqual (scale - std::floor (scale), 0.0f))
                {
                    // lazily add a buffer all around the image for sub-pixel-ness
                    scaledShadowBounds.expand (1, 1);
                }
            }

        private:
            juce::Image singleChannelRender;
            juce::Rectangle<int> scaledShadowBounds;
            juce::Rectangle<int> scaledPathBounds;

            int scaledRadius = 0;
            int scaledSpread = 0;

            // Offsets are separately stored to translate placement in ARGB compositing.
            juce::Point<int> scaledOffset;
        };

    }
}
