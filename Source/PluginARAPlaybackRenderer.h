/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for an ARA playback renderer implementation.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
//==============================================================================
/**
*/

class SharedTimeSliceThread  : public juce::TimeSliceThread
{
public:
    SharedTimeSliceThread()
        : juce::TimeSliceThread (juce::String (JucePlugin_Name) + " ARA Sample Reading Thread")
    {
        startThread (Priority::high);  // Above default priority so playback is fluent, but below realtime
    }
};

class PossiblyBufferedReader
{
public:
    PossiblyBufferedReader() = default;

    explicit PossiblyBufferedReader (std::unique_ptr<juce::BufferingAudioReader> readerIn)
        : setTimeoutFn ([ptr = readerIn.get()] (int ms) { ptr->setReadTimeout (ms); }),
          reader (std::move (readerIn))
    {}

    explicit PossiblyBufferedReader (std::unique_ptr<juce::AudioFormatReader> readerIn)
        : setTimeoutFn(),
          reader (std::move (readerIn))
    {}

    void setReadTimeout (int ms)
    {
        juce::NullCheckedInvocation::invoke (setTimeoutFn, ms);
    }

    juce::AudioFormatReader* get() const { return reader.get(); }

private:
    std::function<void (int)> setTimeoutFn;
    std::unique_ptr<juce::AudioFormatReader> reader;
};

class AmnesiaDemoPlaybackRenderer  : public juce::ARAPlaybackRenderer
{
public:
    //==============================================================================
    using juce::ARAPlaybackRenderer::ARAPlaybackRenderer;

    //==============================================================================
    void prepareToPlay (double sampleRate,
                        int maximumSamplesPerBlock,
                        int numChannels,
                        juce::AudioProcessor::ProcessingPrecision,
                        AlwaysNonRealtime alwaysNonRealtime) override;
    void releaseResources() override;

    //==============================================================================
    bool processBlock (juce::AudioBuffer<float>& buffer,
                       juce::AudioProcessor::Realtime realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

private:
    //==============================================================================
    juce::SharedResourcePointer<SharedTimeSliceThread> sharedTimesliceThread;
    std::map<juce::ARAAudioSource*, PossiblyBufferedReader> audioSourceReaders;
    bool useBufferedAudioSourceReader = true;
    int numChannels = 2;
    double sampleRate = 48000.0;
    int maximumSamplesPerBlock = 128;
    std::unique_ptr<juce::AudioBuffer<float>> tempBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmnesiaDemoPlaybackRenderer)
};
