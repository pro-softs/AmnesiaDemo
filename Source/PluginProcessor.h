/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Utilities.h"
#include "CodebaseAlphaFx.h"

//==============================================================================
/**
*/
class AmnesiaDemoAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    juce::AudioProcessorValueTreeState apvts;

    PlayHeadState playHeadState;
    
    //==============================================================================
    AmnesiaDemoAudioProcessor();
    ~AmnesiaDemoAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioSampleBuffer& buffer, juce::MidiBuffer& midiMessages) override;

    float getParameter (int param) override; ///< Gets a specified parameter value.
    void setParameter (int param, float val) override; ///< Sets a specified parameter value based on the index.
    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    
    int m_numParams; ///< Number of processor parameters.
    float m_delay; ///< Delay time parameter (msecs).
    float m_feedback; ///< Feedback parameter (%).
    float m_mix; ///< Mix parameter (%).
    bool m_bypass; ///< Bypass parameter (true = bypass).

    AlphaSimpleDelay delay;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmnesiaDemoAudioProcessor)
};
