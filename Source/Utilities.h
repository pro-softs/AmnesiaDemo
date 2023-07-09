/*
  ==============================================================================

    Utilities.h
    Created: 9 Jun 2023 3:06:38pm
    Author:  Proshanto Gupta

  ==============================================================================
*/

#pragma once
#include<string>
#include <JuceHeader.h>

class TimeToViewScaling
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void zoomLevelChanged (double newPixelPerSecond) = 0;
    };

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    TimeToViewScaling() = default;

    void zoom (double factor)
    {
        zoomLevelPixelPerSecond = juce::jlimit (minimumZoom, minimumZoom * 32, zoomLevelPixelPerSecond * factor);
        setZoomLevel (zoomLevelPixelPerSecond);
    }

    void setZoomLevel (double pixelPerSecond)
    {
        zoomLevelPixelPerSecond = pixelPerSecond;
        listeners.call ([this] (Listener& l) { l.zoomLevelChanged (zoomLevelPixelPerSecond); });
    }

    int getXForTime (double time) const
    {
        return juce::roundToInt (time * zoomLevelPixelPerSecond);
    }

    double getTimeForX (int x) const
    {
        return x / zoomLevelPixelPerSecond;
    }

private:
    static constexpr auto minimumZoom = 10.0;

    double zoomLevelPixelPerSecond = minimumZoom * 4;
    juce::ListenerList<Listener> listeners;
};

struct PlayHeadState
{
    void update (const juce::Optional<juce::AudioPlayHead::PositionInfo>& info)
    {
        if (info.hasValue())
        {
            isPlaying.store (info->getIsPlaying(), std::memory_order_relaxed);
            timeInSeconds.store (info->getTimeInSeconds().orFallback (0), std::memory_order_relaxed);
            bpm.store(info->getBpm().orFallback (1), std::memory_order_relaxed);
            isLooping.store (info->getIsLooping(), std::memory_order_relaxed);
            const auto loopPoints = info->getLoopPoints();

            if (loopPoints.hasValue())
            {
                loopPpqStart = loopPoints->ppqStart;
                loopPpqEnd = loopPoints->ppqEnd;
            }
        }
        else
        {
            isPlaying.store (false, std::memory_order_relaxed);
            isLooping.store (false, std::memory_order_relaxed);
        }
    }

    std::atomic<bool> isPlaying { false },
                      isLooping { false };
    std::atomic<double> timeInSeconds { 0.0 },
                        loopPpqStart  { 0.0 },
                        bpm {1.0},
                        loopPpqEnd    { 0.0 };
};

//==============================================================================
struct PreviewState
{
    std::atomic<double> previewTime { 0.0 };
    std::atomic<juce::ARAPlaybackRegion*> previewedRegion { nullptr };
};

enum Param { DELAY, FEEDBACK, MIX, BYPASS };

