/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CodebaseAlphaFx.h"

//==============================================================================
AmnesiaDemoAudioProcessor::AmnesiaDemoAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       ),
    apvts (*this, nullptr),
    m_numParams(4),
    m_delay(0.0f),
    m_feedback(0.0f),
    m_mix(50.0f),
    m_bypass(false)
#endif
{
    apvts.state = ValueTree("parent");

    if(!apvts.state.getChildWithName("sections").isValid())
        apvts.state.appendChild(ValueTree("sections"), nullptr);
}

AmnesiaDemoAudioProcessor::~AmnesiaDemoAudioProcessor()
{
}

//==============================================================================
const juce::String AmnesiaDemoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AmnesiaDemoAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AmnesiaDemoAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AmnesiaDemoAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AmnesiaDemoAudioProcessor::getTailLengthSeconds() const
{
    double tail;
    if (getTailLengthSecondsForARA (tail))
        return tail;

    return 0.0;
}

int AmnesiaDemoAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AmnesiaDemoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AmnesiaDemoAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AmnesiaDemoAudioProcessor::getProgramName (int index)
{
    return {};
}

void AmnesiaDemoAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AmnesiaDemoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    delay.reset(sampleRate);
    auto params = delay.getParameters();
    params.delay = m_delay;
    params.feedback = m_feedback;
    params.wetDryMix = m_mix;
    delay.setParameters(params);
    playHeadState.update (juce::nullopt);
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
}

void AmnesiaDemoAudioProcessor::releaseResources()
{
    playHeadState.update (juce::nullopt);
    releaseResourcesForARA();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AmnesiaDemoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AmnesiaDemoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    ignoreUnused (midiMessages);
    
    auto* audioPlayHead = getPlayHead();
    playHeadState.update (audioPlayHead->getPosition());
    
    if (! processBlockForARA (buffer, isRealtime(), audioPlayHead))
        processBlockBypassed (buffer, midiMessages);
    
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    auto* leftChannelData = buffer.getWritePointer (0);
    auto* rightChannelData = buffer.getWritePointer(1);
    
    for (int i = 0; i < buffer.getNumSamples(); i++)
    {
        float inputFrame[2]{ leftChannelData[i], rightChannelData[i] };
        float outputFrame[2];
        
        delay.processAudioFrame(inputFrame, outputFrame, totalNumInputChannels, totalNumOutputChannels);
//        chorus.processAudioFrame(inputFrame, outputFrame, totalNumInputChannels, totalNumOutputChannels);
        
        buffer.setSample(0, i, outputFrame[0]);
        buffer.setSample(1, i, outputFrame[1]);
    }
    
}

void AmnesiaDemoAudioProcessor::processBlockBypassed (AudioSampleBuffer& buffer, MidiBuffer& /*midiMessages*/)
{
    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)
    {
        auto data = buffer.getWritePointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            data[i] = data[i];
        }
    }
}

float AmnesiaDemoAudioProcessor::getParameter (int param)
{
    
    switch (param)
    {
        case DELAY:
            return m_delay;
        case FEEDBACK:
            return m_feedback;
        case MIX:
            return m_mix;
        case BYPASS:
            return m_bypass;
        default:
            return 0;
    }
}

void AmnesiaDemoAudioProcessor::setParameter (int param, float val)
{
    auto params = delay.getParameters();

    switch (param)
    {
        case DELAY:
            m_delay = val;
            if(params.delay != val) params.delay = val;
            break;
        case FEEDBACK:
            m_feedback = val;
            if(params.feedback != val) params.feedback = val;
            break;
        case MIX:
            m_mix= val;
            if(params.wetDryMix != val) params.wetDryMix = val;
            break;
        case BYPASS:
            m_bypass = static_cast<bool>(val);
            break;
        default:
            break;
    }
    
    delay.setParameters(params);
}

//==============================================================================
bool AmnesiaDemoAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AmnesiaDemoAudioProcessor::createEditor()
{
    return new AmnesiaDemoAudioProcessorEditor (*this, apvts);
}

//==============================================================================
void AmnesiaDemoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    auto state = apvts.copyState();
    std::unique_ptr <XmlElement> xml (state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AmnesiaDemoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get())
        if (xmlState -> hasTagName(apvts.state.getType()))
            apvts.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmnesiaDemoAudioProcessor();
}
