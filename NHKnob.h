#pragma once
// NINE HANDS | DSP : potard anime (skill potards-nine-hands-dsp) : ombre, couronne, chapeau et repere tournent ensemble
// au meme angle, flou de mouvement selon la vitesse, reflets fixes (lumiere en haut a gauche a 45 deg).
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace nh
{
struct KnobStyle
{
    float startAngle = juce::degreesToRadians (-135.0f);
    float endAngle   = juce::degreesToRadians ( 135.0f);
    juce::Point<float> shadowOffset { 0.025f, 0.025f };
    float blurMax    = juce::degreesToRadians (40.0f);
    float blurStep   = juce::degreesToRadians (3.0f);
    float blurDecay  = 0.6f;
    float dragPixels = 250.0f;
};

class Knob : public juce::Component, private juce::Timer
{
public:
    struct Layers { juce::Image shadow, crown, cap, wear, marker, highlight; };

    using Style = KnobStyle;

    Knob (juce::RangedAudioParameter& p, Layers l, Style s = {})
        : param (p), layers (std::move (l)), style (s),
          attachment (p, [this] (float v) { target = param.convertTo0to1 (v); }, nullptr)
    {
        attachment.sendInitialUpdate();
        shown = target;
        lastAngle = angleFor (shown);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    std::function<void (const juce::MouseEvent&)> onRightClick;
    std::function<void()> onUserChange;

    bool hitTest (int x, int y) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.42f;
    }
    void resized() override { resizeSettle = frame.isValid() ? 3 : 1; }
    void paint (juce::Graphics& g) override { if (frame.isValid()) g.drawImage (frame, getLocalBounds().toFloat()); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (e); return; }
        dragging = true; lastY = e.position.y; attachment.beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto range = style.dragPixels * (e.mods.isShiftDown() ? 5.0f : 1.0f);
        const auto dy = e.position.y - lastY; lastY = e.position.y;
        setNormalised (target - dy / range, true);
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) attachment.endGesture(); dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) setNormalised (param.getDefaultValue(), false); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { setNormalised (target + w.deltaY * 0.05f, false); }
    bool isDragging() const { return dragging; }

private:
    float angleFor (float v) const { return style.startAngle + v * (style.endAngle - style.startAngle); }

    void setNormalised (float v, bool inGesture)
    {
        target = juce::jlimit (0.0f, 1.0f, v);
        const auto plainV = param.convertFrom0to1 (target);
        if (inGesture) attachment.setValueAsPartOfGesture (plainV);
        else           attachment.setValueAsCompleteGesture (plainV);
        if (onUserChange) onUserChange();
    }

    juce::AffineTransform place (const juce::Image& img, juce::Point<float> c, float a) const
    {
        return juce::AffineTransform::translation (-img.getWidth() * 0.5f, -img.getHeight() * 0.5f)
                   .scaled (1.0f / scale).rotated (a).translated (c.x, c.y);
    }

    void timerCallback() override
    {
        if (++scaleTick >= 8)
        {
            scaleTick = 0;
            const auto s = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this));
            if (std::abs (s - scale) > 0.01f && resizeSettle == 0) resizeSettle = 1;
        }
        if (resizeSettle > 0 && --resizeSettle == 0) { rebuildCache(); dirty = true; }

        shown = target;
        const auto a = angleFor (shown);
        const auto v = a - lastAngle;
        lastAngle = a;
        if (std::abs (v) > 1.0e-5f) blurDir = v > 0.0f ? 1.0f : -1.0f;
        blurSpan = juce::jmax (juce::jmin (std::abs (v), style.blurMax), blurSpan * style.blurDecay);
        if (blurSpan < 0.002f) blurSpan = 0.0f;

        const bool changed = dirty || std::abs (a - drawnAngle) > 1.0e-4f || blurSpan != drawnSpan;
        if (changed && stack.isValid())
        {
            renderFrame (a);
            drawnAngle = a; drawnSpan = blurSpan; dirty = false;
            repaint();
        }
    }

    void renderFrame (float a)
    {
        const int w = juce::roundToInt (getWidth() * scale), h = juce::roundToInt (getHeight() * scale);
        if (w <= 0 || h <= 0) return;
        if (frame.getWidth() != w || frame.getHeight() != h)
        {
            frame   = juce::Image (juce::Image::ARGB, w, h, true);
            scratch = juce::Image (juce::Image::ARGB, w, h, true);
        }
        frame.clear (frame.getBounds());
        juce::Graphics g (frame);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const auto b = getLocalBounds().toFloat();
        const auto d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        if (shadowCache.isValid()) drawBlurred (g, shadowCache, c + style.shadowOffset * d, a);
        drawBlurred (g, stack, c, a);
        if (highlightCache.isValid()) g.drawImage (highlightCache, b.withSizeKeepingCentre (d, d));
    }

    void drawBlurred (juce::Graphics& g, const juce::Image& img, juce::Point<float> c, float a)
    {
        if (blurSpan <= 0.0f) { g.drawImageTransformed (img, place (img, c, a), false); return; }
        const int n = juce::jlimit (4, 16, (int) std::ceil (blurSpan / style.blurStep) + 1);
        scratch.clear (scratch.getBounds());
        {
            juce::Graphics gs (scratch);
            gs.addTransform (juce::AffineTransform::scale (scale));
            gs.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            for (int k = 0; k < n; ++k)
            {
                const auto t = (float) k / (float) (n - 1);
                gs.setOpacity (1.0f / (float) (k + 1));
                gs.drawImageTransformed (img, place (img, c, a - blurDir * blurSpan * t), false);
            }
        }
        g.drawImage (scratch, getLocalBounds().toFloat());
    }

    void rebuildCache()
    {
        scale = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this));
        const int px = juce::roundToInt (juce::jmin (getWidth(), getHeight()) * scale);
        if (px <= 0) return;
        auto fit = [px] (const juce::Image& src)
        {
            if (src.isNull()) return juce::Image();
            juce::Image out (juce::Image::ARGB, px, px, true);
            juce::Graphics g (out);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (src, juce::Rectangle<float> (0.0f, 0.0f, (float) px, (float) px), juce::RectanglePlacement::centred);
            return out;
        };
        shadowCache = fit (layers.shadow);
        highlightCache = fit (layers.highlight);
        stack = juce::Image (juce::Image::ARGB, px, px, true);
        juce::Graphics g (stack);
        for (auto* l : { &layers.crown, &layers.cap, &layers.wear, &layers.marker })
            if (l->isValid()) g.drawImageAt (fit (*l), 0, 0);
    }

    juce::RangedAudioParameter& param;
    Layers layers;
    Style style;
    juce::ParameterAttachment attachment;
    juce::Image stack, shadowCache, highlightCache, frame, scratch;
    float scale = 1.0f, target = 0.0f, shown = 0.0f, lastAngle = 0.0f;
    float blurSpan = 0.0f, blurDir = 1.0f, lastY = 0.0f;
    float drawnAngle = 1.0e9f, drawnSpan = -1.0f;
    int resizeSettle = 0, scaleTick = 0;
    bool dragging = false, dirty = true;
};
} // namespace nh
