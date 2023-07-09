/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"


using namespace juce;

//==============================================================================
/**
*/
class AmnesiaDemoAudioProcessorEditor : public AudioProcessorEditor,
                                        public AudioProcessorEditorARAExtension
{
public:
    AmnesiaDemoAudioProcessorEditor (AmnesiaDemoAudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~AmnesiaDemoAudioProcessorEditor() override;
    
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
private:
    std::unique_ptr<juce::Component> documentView;
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AmnesiaDemoAudioProcessor& audioProcessor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmnesiaDemoAudioProcessorEditor)
};
