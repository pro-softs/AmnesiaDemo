/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "DocumentView.h"

//==============================================================================
AmnesiaDemoAudioProcessorEditor::AmnesiaDemoAudioProcessorEditor (AmnesiaDemoAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessorEditor (&p), AudioProcessorEditorARAExtension (&p), audioProcessor (p)
{
    if (auto* editorView = getARAEditorView())
        documentView = std::make_unique<DocumentView> (*editorView, p.playHeadState, apvts, p);

    addAndMakeVisible (documentView.get());

    // ARA requires that plugin editors are resizable to support tight integration
    // into the host UI
    setResizable (true, false);
    setSize (1500, 800);
}

AmnesiaDemoAudioProcessorEditor::~AmnesiaDemoAudioProcessorEditor()
{
}

//==============================================================================
void AmnesiaDemoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    
    if (! isARAEditorView())
    {
        g.setColour (Colours::white);
        g.setFont (15.0f);
        g.drawFittedText ("ARA host isn't detected. This plugin only supports ARA mode",
                          getLocalBounds(),
                          Justification::centred,
                          1);
    }
}

void AmnesiaDemoAudioProcessorEditor::resized()
{
    if (documentView != nullptr)
        documentView->setBounds (getLocalBounds());
}
