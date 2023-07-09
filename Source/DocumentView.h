#include <JuceHeader.h>
#include "PluginARADocumentController.h"
#include "PluginProcessor.h"
#include "Utilities.h"
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>

using namespace juce;

//==============================================================================

auto colorsList = Array<String>({
    "indianred",       "lightcoral",       "salmon",
        "darksalmon",      "lightsalmon",      "crimson",
        "red",             "darkred",          "pink",
        "lightpink",       "hotpink",          "deeppink",
        "mediumvioletred", "palevioletred",    "coral",
        "tomato",          "orangered",        "darkorange",
        "orange",          "gold",             "yellow",
        "lightyellow",     "lemonchiffon",     "lightgoldenrodyellow",
        "papayawhip",      "moccasin",         "peachpuff",
        "palegoldenrod",   "khaki",            "darkkhaki",
        "lavender",        "thistle",          "plum",
        "violet",          "orchid",           "fuchsia",
        "magenta",         "mediumorchid",     "mediumpurple",
        "rebeccapurple",   "blueviolet",       "darkviolet",
        "darkorchid",      "darkmagenta",      "purple",
        "indigo",          "slateblue",        "darkslateblue",
        "mediumslateblue", "greenyellow",      "chartreuse",
        "lawngreen",       "lime",             "limegreen",
        "palegreen",       "lightgreen",       "mediumspringgreen",
        "springgreen",     "mediumseagreen",   "seagreen",
        "forestgreen",     "green",            "darkgreen",
        "yellowgreen",     "olivedrab",        "olive",
        "darkolivegreen",  "mediumaquamarine", "darkseagreen",
        "lightseagreen",   "darkcyan",         "teal",
        "aqua",            "cyan",             "lightcyan",
        "paleturquoise",   "aquamarine",       "turquoise",
        "mediumturquoise", "darkturquoise",    "cadetblue",
        "steelblue",       "lightsteelblue",   "powderblue",
        "lightblue",       "skyblue",          "lightskyblue",
        "deepskyblue",     "dodgerblue",       "cornflowerblue",
        "royalblue",       "blue",             "mediumblue",
        "darkblue",        "navy",             "midnightblue",
        "cornsilk",        "blanchedalmond",   "bisque",
        "navajowhite"
});

constexpr auto trackHeight = 80;

int sectionIndex = 0;
int selectedIndex = -1;

class CycleMarkerComponent : public Component, public Timer
{
    int index = -1;
    Colour color { Colours::yellow.darker (0.2f) };
    ResizableBorderComponent* resizer;
    
    void paint (Graphics& g) override
    {
        g.setColour (color);
        const auto bounds = getLocalBounds().toFloat();
        
        if(selectedIndex == index)  g.drawRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 6.0f, 10.0f);
        else g.drawRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 6.0f, 2.0f);
    }
    
    void timerCallback() override
    {
        repaint();
    }

public:
    
    CycleMarkerComponent()
    {
        resizer = new ResizableBorderComponent(this, 0);
        addAndMakeVisible(resizer);
        startTimerHz(30);
    }
    
    ~CycleMarkerComponent() override
    {
        stopTimer();
    }
    
    void mouseDoubleClick (const MouseEvent& m) override
    {
        selectedIndex = index;
        repaint();
    }
        
    void setMouseListener(MouseListener* mouseListener)
    {
        resizer->addMouseListener(mouseListener, false);
    }

    void setIndex(int i)
    {
        index = i;
        resizer->setName(String(index));
    }

    void setColor(Colour c)
    {
        color = c;
    }

    void resized() override
    {
        resizer->setBounds(0,0,getWidth(),getHeight());
    }

    Colour getColor()
    {
        return color;
    }
};

struct DelayValue
{
    int delay {1};
    float beatDelay {2};
    float feedback {0.0};
    float mix {1};
};

struct Section
{
    float startPos { 0.0 };
    float endPos { 0.0 };
    Colour color;
    DelayValue delayValues;
    String name {""};
    std::unique_ptr<CycleMarkerComponent> cycleMarker;
    std::unique_ptr<TextEditor> textButton;
};

class SectionUpdateListener
{
public:
    SectionUpdateListener() {};
    virtual ~SectionUpdateListener() {};

    virtual void didAddSection(Section* section, int index) = 0;
    virtual void didRemoveSection() = 0;
};

Array<Section*> sections = Array<Section*>();

Section* addSectionAmnesia(String name, float start, float end,  Colour color, int delay, float beatDelay, float feedback, float mix, MouseListener* listener)
{
    Section *section = new Section();
    
    if(section != nullptr)
    {
        section->startPos = start;
        section->endPos = end;

        section->name = name;

        section->color = color;
        section->delayValues = DelayValue();

        section->delayValues.delay = delay;
        section->delayValues.beatDelay = beatDelay;
        section->delayValues.feedback = feedback;
        section->delayValues.mix = mix;

        section->cycleMarker = std::make_unique<CycleMarkerComponent>();
        section->cycleMarker->setColor(color);
        section->cycleMarker->setIndex(sectionIndex);
        section->cycleMarker->setMouseListener(listener);

        section->textButton = std::make_unique<TextEditor>();
        (*section->textButton).setText(section->name);

        sections.add(section);
        selectedIndex = sectionIndex;
        
        ++sectionIndex;
        return section;
    }
    
    return section;
};

void removeSectionAmensia(int index)
{
    auto *section = sections[index];
    (*section->cycleMarker).setVisible(false);
    (*section->textButton).setVisible(false);
    sections.remove(index);
    sectionIndex--;
}

class ZoomControls : public Component
{
public:
    ZoomControls()
    {
        addAndMakeVisible (zoomInButton);
        addAndMakeVisible (zoomOutButton);
    }

    void setZoomInCallback  (std::function<void()> cb)   { zoomInButton.onClick  = std::move (cb); }
    void setZoomOutCallback (std::function<void()> cb)   { zoomOutButton.onClick = std::move (cb); }

    void resized() override
    {
        FlexBox fb;
        fb.justifyContent = FlexBox::JustifyContent::flexEnd;

        for (auto* button : { &zoomInButton, &zoomOutButton })
            fb.items.add (FlexItem (*button).withHeight (30.0f).withWidth (30.0f).withMargin ({ 5, 5, 5, 0 }));

        fb.performLayout (getLocalBounds());
    }

private:
    TextButton zoomInButton { "+" }, zoomOutButton { "-" };
};

class PlayheadPositionLabel : public Label,
                              private Timer
{
public:
    PlayheadPositionLabel (PlayHeadState& playHeadStateIn, AmnesiaDemoAudioProcessor& p)
        : playHeadState (playHeadStateIn),
          processor(p)
    {
        startTimerHz (30);
    }

    ~PlayheadPositionLabel() override
    {
        stopTimer();
    }

private:
    void timerCallback() override
    {
        const auto timePosition = playHeadState.timeInSeconds.load (std::memory_order_relaxed);

        auto text = timeToTimecodeString (timePosition);

        if (playHeadState.isPlaying.load (std::memory_order_relaxed))
            text += " (playing)";
        else
            text += " (stopped)";

        setText (text, NotificationType::dontSendNotification);
        
        bool isASection = false;

        for(auto *section: sections)
        {
            int index = sections.indexOf(section);

            if(section->startPos <= timePosition && timePosition <= section->endPos)
            {
                if(selectedIndex != index || (selectedIndex == 0 && index == 0))
                {
                    selectedIndex = index;
                }
                
                processor.setParameter(DELAY, (section->delayValues.beatDelay) * 60.0 / playHeadState.bpm);
                processor.setParameter(FEEDBACK, section->delayValues.feedback);
                processor.setParameter(MIX, section->delayValues.mix);
                
                isASection = true;
                
                break;
            }
        }
        
        if(!isASection)
        {
//            processor.setParameter(DELAY, 0);
            processor.setParameter(FEEDBACK, 0);
        }
    }

    // Copied from AudioPluginDemo.h: quick-and-dirty function to format a timecode string
    static String timeToTimecodeString (double seconds)
    {
        auto millisecs = roundToInt (seconds * 1000.0);
        auto absMillisecs = std::abs (millisecs);

        return String::formatted ("%02d:%02d:%02d.%03d",
                                  millisecs / 3600000,
                                  (absMillisecs / 60000) % 60,
                                  (absMillisecs / 1000)  % 60,
                                  absMillisecs % 1000);
    }

    PlayHeadState& playHeadState;
    AmnesiaDemoAudioProcessor& processor;
};

class SectionList: public Component,
                   public SectionUpdateListener
{
public:
    SectionList()
    {
    }
    
    ~SectionList()
    {
    }

    void resized() override
    {
        FlexBox fb;                                               // [1]
        fb.flexDirection = FlexBox::Direction::row;
        fb.flexWrap = FlexBox::Wrap::wrap;                        // [2]
        fb.alignContent = FlexBox::AlignContent::flexStart;
        
        for (auto *section: sections)
        {
            fb.items.add (FlexItem (*section->textButton).withHeight (30.0f).withWidth (100.0f).withMargin ({ 5, 5, 5, 5 }));
        }
        
        fb.performLayout (getLocalBounds());
    }
    
private:
    
    class MyTextEditorListener : public TextEditor::Listener
    {
    public:
        int index = -1;
        
        void textEditorTextChanged (TextEditor& textEditor) override
        {
            std::cout << textEditor.getText() << std::endl;
        }
    };
    
    MyTextEditorListener listener;
    
    void didAddSection(Section* section, int index) override
    {
        listener = MyTextEditorListener();
        listener.index = index;

        addAndMakeVisible(*section->textButton);

        (*section->textButton).setColour(TextEditor::backgroundColourId, section->color);
        (*section->textButton).addListener(&listener);

        resized();
    }

    void didRemoveSection() override
    {
        
    }
};

void updateValueTree(Section* section, ValueTree& sectionTree)
{
    ValueTree point = sectionTree.getChildWithName (section->name);
    point.setProperty("color", section->color.toString(), nullptr);
    point.setProperty("startPos", (float) section->startPos, nullptr);
    point.setProperty("endPos", (float) section->endPos, nullptr);
    
    ValueTree delay = point.getChildWithName ("delays");
    delay.setProperty("delay", section->delayValues.delay, nullptr);
    delay.setProperty("beatDelay", section->delayValues.beatDelay, nullptr);
    delay.setProperty("feedback", section->delayValues.feedback, nullptr);
    delay.setProperty("mix", section->delayValues.mix, nullptr);
}

class DelayComponent : public Component, public Slider::Listener, public Timer
{
public:
    ValueTree& sectionTree;
    Label m_delayLabel; ///< Delay knob label.
    ComboBox m_delayKnob; ///< Knob for adjusting the delay time (msecs).
    Label m_feedbackLabel; ///< Feedback knob label.
    Slider m_feedbackKnob; ///< Knob for adjusting the feedback (%).
    Label m_mixLabel; ///< Mix knob label.
    Slider m_mixKnob; ///< Knob for adjusting the wet/dry mix (%).
    
    DelayComponent(ValueTree& vt) : Slider::Listener(), sectionTree(vt),
    m_delayLabel ("delay label", "Delay"),
    m_delayKnob ("delay knob"),
    m_feedbackLabel ("feedback", "Feedback"),
    m_feedbackKnob ("feedback knob"),
    m_mixLabel ("mix label", "Mix"),
    m_mixKnob ("mix knob")
    {
        // Set up the delay time control.
        addAndMakeVisible (m_delayLabel);
        m_delayLabel.setFont (18.00f);
        m_delayLabel.setJustificationType (Justification::centred);
        m_delayLabel.attachToComponent (&m_delayKnob, false);
        addAndMakeVisible (m_delayKnob);

        m_delayKnob.setTooltip ("Delay");
        m_delayKnob.setColour(juce::Slider::rotarySliderFillColourId, Colours::white);
        
        m_delayKnob.addItem ("Half",  1);
        m_delayKnob.addItem ("8th",   2);
        m_delayKnob.addItem ("Quarter", 3);
        m_delayKnob.addItem ("16th", 4);
        m_delayKnob.onChange = [this] { beatMenuChanged(); };
        if(selectedIndex > -1) m_delayKnob.setSelectedId (sections[selectedIndex]->delayValues.delay);

        // Set up the feedback control.
        addAndMakeVisible (m_feedbackLabel);

        m_feedbackLabel.setTooltip ("Feedback (%)");
        m_feedbackLabel.setFont (18.00f);
        m_feedbackLabel.setJustificationType (Justification::centred);
        m_feedbackLabel.attachToComponent (&m_feedbackKnob, false);

        addAndMakeVisible (m_feedbackKnob);

        m_feedbackKnob.setTooltip ("Feedback");
        m_feedbackKnob.setColour(juce::Slider::rotarySliderFillColourId, Colours::white);
        m_feedbackKnob.setRange (0, 0.98);
        m_feedbackKnob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        m_feedbackKnob.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 20);
        m_feedbackKnob.setTextValueSuffix (" %");
        m_feedbackKnob.addListener (this);
        if(selectedIndex > -1) m_feedbackKnob.setValue(sections[selectedIndex]->delayValues.feedback);

        // Set up the mix control.
        addAndMakeVisible (m_mixLabel);
        m_mixLabel.setTooltip ("Wet/dry mix (%)");
        m_mixLabel.setFont (18.00f);
        m_mixLabel.setJustificationType (Justification::centred);
        m_mixLabel.attachToComponent (&m_mixKnob, false);

        addAndMakeVisible (m_mixKnob);
        m_mixKnob.setTooltip ("Mix");
        m_mixKnob.setColour(juce::Slider::rotarySliderFillColourId, Colours::white);
        m_mixKnob.setRange (0, 1);
        
        m_mixKnob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        m_mixKnob.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 20);
        m_mixKnob.setTextValueSuffix (" %");
        m_mixKnob.addListener (this);
        
        if(selectedIndex > -1) m_mixKnob.setValue(sections[selectedIndex]->delayValues.mix);

        startTimerHz(30);
    }
    
    ~DelayComponent() override
    {
        stopTimer();
    }
    
    void timerCallback() override
    {
        if(selectedIndex >= 0)
        {
            m_delayKnob.setSelectedId(sections[selectedIndex]->delayValues.delay);
            m_feedbackKnob.setValue (sections[selectedIndex]->delayValues.feedback, dontSendNotification);
            m_mixKnob.setValue (sections[selectedIndex]->delayValues.mix, dontSendNotification);
        }
    }
    
    void beatMenuChanged()
    {
        if(selectedIndex >= 0)
        {
            sections[selectedIndex]->delayValues.delay = m_delayKnob.getSelectedId();
            switch(m_delayKnob.getSelectedId())
            {
                case 1:
                    sections[selectedIndex]->delayValues.beatDelay = 2;
                    break;
                case 2:
                    sections[selectedIndex]->delayValues.beatDelay = 0.5;
                    break;
                case 3:
                    sections[selectedIndex]->delayValues.beatDelay = 1;
                case 4:
                    sections[selectedIndex]->delayValues.beatDelay = 0.25;
                    break;
            }
            
            updateValueTree(sections[selectedIndex], sectionTree);
        }
    }

    void resized() override
    {
        m_delayKnob.setBounds (proportionOfWidth(0.35), proportionOfHeight(0.18), proportionOfWidth(0.4), proportionOfHeight(0.1));
        m_feedbackKnob.setBounds (proportionOfWidth(0.15), proportionOfHeight(0.45), proportionOfWidth(0.4), proportionOfHeight(0.5));
        m_mixKnob.setBounds (proportionOfWidth(0.48), proportionOfHeight(0.45), proportionOfWidth(0.4), proportionOfHeight(0.5));
    }

    void paint (Graphics& g) override
    {
        const auto backgroundColour = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.setColour (backgroundColour.brighter());
        g.fillRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f);
        g.setColour (backgroundColour.contrasting());
        g.drawRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f, 1.0f);
    }

    void sliderValueChanged (Slider* slider) override
    {
        if (slider == &m_feedbackKnob)
        {
            if(selectedIndex >= 0)
            {
                sections[selectedIndex]->delayValues.feedback = m_feedbackKnob.getValue();
            }
        }
        else if (slider == &m_mixKnob)
        {
            if(selectedIndex >= 0)
            {
                sections[selectedIndex]->delayValues.mix = m_mixKnob.getValue();
            }
        }
        
        updateValueTree(sections[selectedIndex], sectionTree);
    }
};

class SectionLayoutViewport : public Viewport
{
public:
    SectionLayoutViewport()
    {
    }
    
    std::function<void (Rectangle<int>)> onVisibleAreaChanged;

private:
    
    void paint (Graphics& g) override
    {
        const auto backgroundColour = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.setColour (backgroundColour.brighter());
        g.fillRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f);
        g.setColour (backgroundColour.contrasting());
        g.drawRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f, 1.0f);
    }

    void visibleAreaChanged (const Rectangle<int>& newVisibleArea) override
    {
        NullCheckedInvocation::invoke (onVisibleAreaChanged, newVisibleArea);
    }
};

class RulersView : public Component,
                   public SettableTooltipClient,
                   private TimeToViewScaling::Listener,
                   private ARAMusicalContext::Listener
{
public:
    RulersView (PlayHeadState& playHeadStateIn, TimeToViewScaling& timeToViewScalingIn, ARADocument& document)
        : playHeadState (playHeadStateIn), timeToViewScaling (timeToViewScalingIn), araDocument (document)
    {
        timeToViewScaling.addListener (this);
    }

    ~RulersView() override
    {
        timeToViewScaling.removeListener (this);
        selectMusicalContext (nullptr);
    }

    void paint (Graphics& g) override
    {
        auto drawBounds = g.getClipBounds();
        const auto drawStartTime = timeToViewScaling.getTimeForX (drawBounds.getX());
        const auto drawEndTime   = timeToViewScaling.getTimeForX (drawBounds.getRight());

        const auto bounds = getLocalBounds();

        g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).contrasting());
        g.drawRect (bounds);

        const auto rulerHeight = bounds.getHeight();
        g.drawRect (drawBounds.getX(), rulerHeight, drawBounds.getRight(), rulerHeight);
        g.setFont (Font (12.0f));

        const int lightLineWidth = 1;
        const int heavyLineWidth = 3;

        // time ruler: one tick for each second
        {
            RectangleList<int> rects;

            for (auto time = std::floor (drawStartTime); time <= drawEndTime; time += 1.0)
            {
                const int lineWidth  = (std::fmod (time, 60.0) <= 0.001) ? heavyLineWidth : lightLineWidth;
                const int lineHeight = (std::fmod (time, 10.0) <= 0.001) ? rulerHeight : rulerHeight / 2;
                rects.addWithoutMerging (Rectangle<int> (timeToViewScaling.getXForTime (time) - lineWidth / 2,
                                                         bounds.getHeight() - lineHeight,
                                                         lineWidth,
                                                         lineHeight));
            }

            g.fillRectList (rects);
        }
        
    }

    void mouseDoubleClick (const MouseEvent&) override
    {
        if (auto* playbackController = araDocument.getDocumentController()->getHostPlaybackController())
        {
            if (! playHeadState.isPlaying.load (std::memory_order_relaxed))
                playbackController->requestStartPlayback();
        }
    }

    void selectMusicalContext (ARAMusicalContext* newSelectedMusicalContext)
    {
        if (auto* oldSelection = std::exchange (selectedMusicalContext, newSelectedMusicalContext);
            oldSelection != selectedMusicalContext)
        {
            if (oldSelection != nullptr)
                oldSelection->removeListener (this);

            if (selectedMusicalContext != nullptr)
                selectedMusicalContext->addListener (this);

            repaint();
        }
    }

    void zoomLevelChanged (double) override
    {
        repaint();
    }

    void doUpdateMusicalContextContent (ARAMusicalContext*, ARAContentUpdateScopes) override
    {
        repaint();
    }

private:
    PlayHeadState& playHeadState;
    TimeToViewScaling& timeToViewScaling;
    ARADocument& araDocument;
    ARAMusicalContext* selectedMusicalContext = nullptr;
};

class RulersHeader : public Component
{
public:
    RulersHeader()
    {
        timeLabel.setText ("Time", NotificationType::dontSendNotification);
        addAndMakeVisible (timeLabel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const auto rulerHeight = bounds.getHeight();

        for (auto* label : { &timeLabel })
            label->setBounds (bounds.removeFromTop (rulerHeight));
    }

    void paint (Graphics& g) override
    {
        auto bounds = getLocalBounds();
        const auto rulerHeight = bounds.getHeight();
        g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).contrasting());
        g.drawRect (bounds);
        bounds.removeFromTop (rulerHeight);
        g.drawRect (bounds.removeFromTop (rulerHeight));
    }

private:
    Label timeLabel;
};

//==============================================================================

class TrackHeader : public Component,
                    private ARARegionSequence::Listener,
                    private ARAEditorView::Listener
{
public:
    TrackHeader (ARAEditorView& editorView, ARARegionSequence& regionSequenceIn)
        : araEditorView (editorView), regionSequence (regionSequenceIn)
    {
        updateTrackName (regionSequence.getName());
        onNewSelection (araEditorView.getViewSelection());

        addAndMakeVisible (trackNameLabel);

        regionSequence.addListener (this);
        araEditorView.addListener (this);
    }

    ~TrackHeader() override
    {
        araEditorView.removeListener (this);
        regionSequence.removeListener (this);
    }

    void willUpdateRegionSequenceProperties (ARARegionSequence*, ARARegionSequence::PropertiesPtr newProperties) override
    {
        if (regionSequence.getName() != newProperties->name)
            updateTrackName (newProperties->name);
        if (regionSequence.getColor() != newProperties->color)
            repaint();
    }

    void resized() override
    {
        trackNameLabel.setBounds (getLocalBounds().reduced (2));
    }

    void paint (Graphics& g) override
    {
        const auto backgroundColour = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.setColour (isSelected ? backgroundColour.brighter() : backgroundColour);
        g.fillRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f);
        g.setColour (backgroundColour.contrasting());
        g.drawRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f, 1.0f);

        if (auto colour = regionSequence.getColor())
        {
            g.setColour (convertARAColour (colour));
            g.fillRect (getLocalBounds().removeFromTop (16).reduced (6));
            g.fillRect (getLocalBounds().removeFromBottom (16).reduced (6));
        }
    }

    void onNewSelection (const ARAViewSelection& viewSelection) override
    {
        const auto& selectedRegionSequences = viewSelection.getRegionSequences();
        const bool selected = std::find (selectedRegionSequences.begin(), selectedRegionSequences.end(), &regionSequence) != selectedRegionSequences.end();

        if (selected != isSelected)
        {
            isSelected = selected;
            repaint();
        }
    }

private:
    void updateTrackName (ARA::ARAUtf8String optionalName)
    {
        trackNameLabel.setText (optionalName ? optionalName : "No track name",
                                NotificationType::dontSendNotification);
    }

    ARAEditorView& araEditorView;
    ARARegionSequence& regionSequence;
    Label trackNameLabel;
    bool isSelected = false;
};

class VerticalLayoutViewportContent : public Component
{
public:
    void resized() override
    {
        auto bounds = getLocalBounds();

        for (auto* component : getChildren())
        {
            if((String) component->getName() == "timer")
                component->setBounds (bounds.removeFromTop (20));
            else
                component->setBounds (bounds.removeFromTop (trackHeight));
            component->resized();
        }
    }
};

class VerticalLayoutViewport : public Viewport
{
public:
    VerticalLayoutViewport()
    {
        setViewedComponent (&content, false);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).brighter());
    }
    
    std::function<void (Rectangle<int>)> onVisibleAreaChanged;

    VerticalLayoutViewportContent content;

private:
    void visibleAreaChanged (const Rectangle<int>& newVisibleArea) override
    {
        NullCheckedInvocation::invoke (onVisibleAreaChanged, newVisibleArea);
    }
};

class OverlayComponent : public Component,
                         private Timer,
                         private TimeToViewScaling::Listener
{
public:
    class PlayheadMarkerComponent : public Component
    {
        void paint (Graphics& g) override { g.fillAll (Colours::yellow.darker (0.2f)); }
    };

    OverlayComponent (PlayHeadState& playHeadStateIn, TimeToViewScaling& timeToViewScalingIn)
        : playHeadState (playHeadStateIn), timeToViewScaling (timeToViewScalingIn)
    {
        addChildComponent (playheadMarker);
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);

        timeToViewScaling.addListener (this);
    }

    ~OverlayComponent() override
    {
        timeToViewScaling.removeListener (this);

        stopTimer();
    }

    void resized() override
    {
        updatePlayHeadPosition();
    }

    void setHorizontalOffset (int offset)
    {
        horizontalOffset = offset;
    }

    void setSelectedTimeRange (std::optional<ARA::ARAContentTimeRange> timeRange)
    {
        selectedTimeRange = timeRange;
        repaint();
    }

    void zoomLevelChanged (double) override
    {
        updatePlayHeadPosition();
        repaint();
    }

    void paint (Graphics& g) override
    {
        if (selectedTimeRange)
        {
            auto bounds = getLocalBounds();
            bounds.setLeft (timeToViewScaling.getXForTime (selectedTimeRange->start));
            bounds.setRight (timeToViewScaling.getXForTime (selectedTimeRange->start + selectedTimeRange->duration));
            g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).brighter().withAlpha (0.3f));
            g.fillRect (bounds);
            g.setColour (Colours::whitesmoke.withAlpha (0.5f));
            g.drawRect (bounds);
        }
    }

private:
    void updatePlayHeadPosition()
    {
        if (playHeadState.isPlaying.load (std::memory_order_relaxed))
        {
            const auto markerX = timeToViewScaling.getXForTime (playHeadState.timeInSeconds.load (std::memory_order_relaxed));
            const auto playheadLine = getLocalBounds().withTrimmedLeft ((int) (markerX - markerWidth / 2.0) - horizontalOffset)
                                                      .removeFromLeft ((int) markerWidth);
            playheadMarker.setVisible (true);
            playheadMarker.setBounds (playheadLine);
        }
        else
        {
            playheadMarker.setVisible (false);
        }
    }

    void timerCallback() override
    {
        updatePlayHeadPosition();
    }

    static constexpr double markerWidth = 2.0;

    PlayHeadState& playHeadState;
    TimeToViewScaling& timeToViewScaling;
    int horizontalOffset = 0;
    std::optional<ARA::ARAContentTimeRange> selectedTimeRange;
    PlayheadMarkerComponent playheadMarker;
};

//==============================================================================

struct WaveformCache : private ARAAudioSource::Listener
{
    WaveformCache() : thumbnailCache (20)
    {
    }

    ~WaveformCache() override
    {
        for (const auto& entry : thumbnails)
        {
            entry.first->removeListener (this);
        }
    }

    //==============================================================================
    void willDestroyAudioSource (ARAAudioSource* audioSource) override
    {
        removeAudioSource (audioSource);
    }

    AudioThumbnail& getOrCreateThumbnail (ARAAudioSource* audioSource)
    {
        const auto iter = thumbnails.find (audioSource);

        if (iter != std::end (thumbnails))
            return *iter->second;

        auto thumb = std::make_unique<AudioThumbnail> (128, dummyManager, thumbnailCache);
        auto& result = *thumb;

        ++hash;
        thumb->setReader (new ARAAudioSourceReader (audioSource), hash);

        audioSource->addListener (this);
        thumbnails.emplace (audioSource, std::move (thumb));
        return result;
    }

private:
    void removeAudioSource (ARAAudioSource* audioSource)
    {
        audioSource->removeListener (this);
        thumbnails.erase (audioSource);
    }

    int64 hash = 0;
    AudioFormatManager dummyManager;
    AudioThumbnailCache thumbnailCache;
    std::map<ARAAudioSource*, std::unique_ptr<AudioThumbnail>> thumbnails;
};

class PlaybackRegionView : public Component,
                           public ChangeListener,
                           private TimeToViewScaling::Listener,
                           public SettableTooltipClient,
                           private ARAAudioSource::Listener,
                           private ARAPlaybackRegion::Listener,
                           private ARAEditorView::Listener,
                           private Timer
{
public:
    PlaybackRegionView (SectionUpdateListener& updator, ARAEditorView& editorView, TimeToViewScaling& timeToViewScalingIn, ARAPlaybackRegion& region, WaveformCache& cache, ValueTree& apvts) : timeToViewScaling(timeToViewScalingIn), araEditorView (editorView), playbackRegion (region), waveformCache (cache), updateListener(updator), sectionTree(apvts)
    {
        setName("playback");
        startTimerHz (30);

        timeToViewScaling.addListener (this);
        
        auto* audioSource = playbackRegion.getAudioModification()->getAudioSource();
        
        waveformCache.getOrCreateThumbnail (audioSource).addChangeListener (this);
        
        audioSource->addListener (this);
        playbackRegion.addListener (this);
        araEditorView.addListener (this);
        
        addChildComponent (cycleMarker);
        cycleMarker.setInterceptsMouseClicks (false, false);
        
        setTooltip ("Drag horizontal range to set sections.");
        
        for(auto *section: sections)
        {
            refreshView(section, sections.indexOf(section));
        }
    }
    
    ~PlaybackRegionView() override
    {
        stopTimer();
        auto* audioSource = playbackRegion.getAudioModification()->getAudioSource();
        
        audioSource->removeListener (this);
        playbackRegion.removeListener (this);
        araEditorView.removeListener (this);
        timeToViewScaling.removeListener (this);
        
        waveformCache.getOrCreateThumbnail (audioSource).removeChangeListener (this);
    }
    
    void mouseDrag (const MouseEvent& m) override
    {
        if(m.eventComponent->getName().indexOf("playback") > -1)
        {
            isDraggingCycle = true;
            
            int _index = sectionIndex;
            if(_index >= colorsList.size()) _index -= colorsList.size();
            
            cycleMarker.setColor(Colours::findColourForName(colorsList[_index], Colours::white));
            
            auto cycleRect = getBounds();
            cycleRect.setLeft  (jmin (m.getMouseDownX(), m.x));
            cycleRect.setRight (jmax (m.getMouseDownX(), m.x));
            
            cycleMarker.setVisible (true);
            cycleMarker.setBounds (cycleRect);
        }
    }
    
    void mouseUp (const MouseEvent& m) override
    {
        if(m.eventComponent->getName().indexOf("playback") > -1)
        {
            startTime = timeToViewScaling.getTimeForX (jmin (m.getMouseDownX(), m.x));
            endTime   = timeToViewScaling.getTimeForX (jmax (m.getMouseDownX(), m.x));

            if(endTime - startTime > 0.4) {
                String name = "Section ";
                name += String(sectionIndex + 1);
                
                auto *section = addSectionAmnesia(name, startTime, endTime, cycleMarker.getColor(), 1, 0, 0,0, this);
                refreshView(section, sectionIndex - 1);
                
                addToValueTree(section, sectionIndex);
            }

            cycleMarker.setVisible(false);
            isDraggingCycle = false;
        }
        else
        {
            int index = m.eventComponent->getName().getIntValue();
            Section *section = sections[index];

            if(section != nullptr)
            {
                if(abs(timeToViewScaling.getTimeForX (m.getEventRelativeTo(this).getMouseDownX()) - section->startPos) < 0.1)
                    section->startPos = timeToViewScaling.getTimeForX (m.getEventRelativeTo(this).x);
                else
                    section->endPos = timeToViewScaling.getTimeForX (m.getEventRelativeTo(this).x);

                if(section->endPos - section->startPos > 0.01) {
                    updateValueTree(section, sectionTree);
                    resized();
                } else {
                    removeSectionAmensia(index);
        
                    removeFromValueTree(index);
        
                    resized();
//                    AlertWindow::showYesNoCancelBox (
//                        AlertWindow::InfoIcon,
//                        "Save changes to the current project?",
//                        juce::String(sectionIndex),
//                        "Save and Continue",
//                        "Cancel",
//                        "Discard Changes", this, nullptr);
                }
            }
        }
    }
    
    void changeListenerCallback (ChangeBroadcaster*) override
    {
        repaint();
    }
    
    void didEnableAudioSourceSamplesAccess (ARAAudioSource*, bool) override
    {
        repaint();
    }
    
    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion*,
                                             ARAPlaybackRegion::PropertiesPtr newProperties) override
    {
        if (playbackRegion.getName() != newProperties->name
            || playbackRegion.getColor() != newProperties->color)
        {
            repaint();
        }
    }
    
    void didUpdatePlaybackRegionContent (ARAPlaybackRegion*, ARAContentUpdateScopes) override
    {
        repaint();
    }
    
    void onNewSelection (const ARAViewSelection& viewSelection) override
    {
        const auto& selectedPlaybackRegions = viewSelection.getPlaybackRegions();
        const bool selected = std::find (selectedPlaybackRegions.begin(), selectedPlaybackRegions.end(), &playbackRegion) != selectedPlaybackRegions.end();
        if (selected != isSelected)
        {
            isSelected = selected;
            repaint();
        }
    }
    
    void paint (Graphics& g) override
    {
        g.fillAll (convertOptionalARAColour (playbackRegion.getEffectiveColor(), Colours::black));
        
        const auto* audioModification = playbackRegion.getAudioModification();
        g.setColour (Colours::darkgrey.brighter());
        
        if (audioModification->getAudioSource()->isSampleAccessEnabled())
        {
            auto& thumbnail = waveformCache.getOrCreateThumbnail (playbackRegion.getAudioModification()->getAudioSource());
            thumbnail.drawChannels (g,
                                    getLocalBounds(),
                                    playbackRegion.getStartInAudioModificationTime(),
                                    playbackRegion.getEndInAudioModificationTime(),
                                    1.0f);
        }
        else
        {
            g.setFont (Font (12.0f));
            g.drawText ("Audio Access Disabled", getLocalBounds(), Justification::centred);
        }
        
        g.setColour (Colours::white.withMultipliedAlpha (0.9f));
        g.setFont (Font (12.0f));
        g.drawText (convertOptionalARAString (playbackRegion.getEffectiveName()),
                    getLocalBounds(),
                    Justification::topLeft);
        
        g.setColour (Colours::white);
        g.drawRect (getLocalBounds());
        
        addChildComponent (cycleMarker);
    }
    
    void resized() override
    {        
        for (auto *section: sections)
        {
            auto cycleRect = getBounds();
            cycleRect.setLeft  (timeToViewScaling.getXForTime (section->startPos));
            cycleRect.setRight (timeToViewScaling.getXForTime (section->endPos));
            
            section->cycleMarker->setBounds(cycleRect);
        }
    }
    
private:
    void timerCallback() override
    {
    }

    AmnesiaDemoDocumentController* getDocumentController() const
    {
        return ARADocumentControllerSpecialisation::getSpecialisedDocumentController<AmnesiaDemoDocumentController> (playbackRegion.getDocumentController());
    }
    
    void addToValueTree(Section* section, int index)
    {
        ValueTree point (section->name);
        point.setProperty("name", section->name, nullptr);
        point.setProperty("color", section->color.toString(), nullptr);
        point.setProperty("startPos", (float) section->startPos, nullptr);
        point.setProperty("endPos", (float) section->endPos, nullptr);
        
        ValueTree delay ("delays");
        delay.setProperty("delay", section->delayValues.delay, nullptr);
        delay.setProperty("beatDelay", section->delayValues.beatDelay, nullptr);
        delay.setProperty("feedback", section->delayValues.feedback, nullptr);
        delay.setProperty("mix", section->delayValues.mix, nullptr);
        
        point.appendChild(delay, nullptr);

        sectionTree.appendChild(point, nullptr);
    }
    
    void removeFromValueTree(int index)
    {
        sectionTree.removeChild(index, nullptr);
    }

    void refreshView(Section* section, int index)
    {
        addAndMakeVisible(*section->cycleMarker);

        resized();
        updateListener.didAddSection(section, index);
    }
    
    void zoomLevelChanged (double) override
    {
        repaint();
    }
    
    TimeToViewScaling& timeToViewScaling;
    ARAEditorView& araEditorView;
    ARAPlaybackRegion& playbackRegion;
    WaveformCache& waveformCache;
    bool isSelected = false;
    
    float startTime = 0.0f, endTime = 0.0f;
    
    SectionUpdateListener& updateListener;
    
    CycleMarkerComponent cycleMarker;
    bool isDraggingCycle = false;
    ValueTree& sectionTree;
};

class RegionSequenceView : public Component,
                           public ChangeBroadcaster,
                           private TimeToViewScaling::Listener,
                           private ARARegionSequence::Listener,
                           private ARAPlaybackRegion::Listener
{
public:
    RegionSequenceView (SectionUpdateListener& updator, ARAEditorView& editorView, TimeToViewScaling& scaling, ARARegionSequence& rs, WaveformCache& cache, ValueTree& tree) : araEditorView (editorView), timeToViewScaling (scaling), regionSequence (rs), waveformCache (cache), apvts (tree), updateListener(updator)
    {
        regionSequence.addListener (this);

        for (auto* playbackRegion : regionSequence.getPlaybackRegions())
            createAndAddPlaybackRegionView (playbackRegion);

        updatePlaybackDuration();

        timeToViewScaling.addListener (this);
    }

    ~RegionSequenceView() override
    {
        timeToViewScaling.removeListener (this);

        regionSequence.removeListener (this);

        for (const auto& it : playbackRegionViews)
            it.first->removeListener (this);
    }

    //==============================================================================
    // ARA Document change callback overrides
    void willUpdateRegionSequenceProperties (ARARegionSequence*,
                                             ARARegionSequence::PropertiesPtr newProperties) override
    {
        if (regionSequence.getColor() != newProperties->color)
        {
            for (auto& pbr : playbackRegionViews)
                pbr.second->repaint();
        }
    }

    void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence*,
                                                     ARAPlaybackRegion* playbackRegion) override
    {
        playbackRegion->removeListener (this);
        removeChildComponent (playbackRegionViews[playbackRegion].get());
        playbackRegionViews.erase (playbackRegion);
        updatePlaybackDuration();
    }

    void didAddPlaybackRegionToRegionSequence (ARARegionSequence*, ARAPlaybackRegion* playbackRegion) override
    {
        createAndAddPlaybackRegionView (playbackRegion);
        updatePlaybackDuration();
    }

    void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) override
    {
        playbackRegion->removeListener (this);
        removeChildComponent (playbackRegionViews[playbackRegion].get());
        playbackRegionViews.erase (playbackRegion);
        updatePlaybackDuration();
    }

    void didUpdatePlaybackRegionProperties (ARAPlaybackRegion*) override
    {
        updatePlaybackDuration();
    }

    void zoomLevelChanged (double) override
    {
        resized();
    }

    void resized() override
    {
        for (auto& pbr : playbackRegionViews)
        {
            const auto playbackRegion = pbr.first;
            pbr.second->setBounds (
                getLocalBounds()
                    .withTrimmedLeft (timeToViewScaling.getXForTime (playbackRegion->getStartInPlaybackTime()))
                    .withWidth (timeToViewScaling.getXForTime (playbackRegion->getDurationInPlaybackTime())));
        }
    }

    auto getPlaybackDuration() const noexcept
    {
        return playbackDuration;
    }

private:
    void createAndAddPlaybackRegionView (ARAPlaybackRegion* playbackRegion)
    {
        playbackRegionViews[playbackRegion] = std::make_unique<PlaybackRegionView> (updateListener, araEditorView,
                                                                                    timeToViewScaling,
                                                                                    *playbackRegion,
                                                                                    waveformCache, apvts);
        playbackRegion->addListener (this);
        addAndMakeVisible (*playbackRegionViews[playbackRegion]);
    }

    void updatePlaybackDuration()
    {
        const auto iter = std::max_element (
            playbackRegionViews.begin(),
            playbackRegionViews.end(),
            [] (const auto& a, const auto& b) { return a.first->getEndInPlaybackTime() < b.first->getEndInPlaybackTime(); });

        playbackDuration = iter != playbackRegionViews.end() ? iter->first->getEndInPlaybackTime()
                                                             : 0.0;

        sendChangeMessage();
    }

    ARAEditorView& araEditorView;
    TimeToViewScaling& timeToViewScaling;
    ARARegionSequence& regionSequence;
    WaveformCache& waveformCache;
    ValueTree& apvts;
    SectionUpdateListener& updateListener;
    std::unordered_map<ARAPlaybackRegion*, std::unique_ptr<PlaybackRegionView>> playbackRegionViews;
    double playbackDuration = 0.0;
};

//==============================================================================

class DocumentView  : public Component,
                      public ChangeListener,
                      public ARAMusicalContext::Listener,
                      private ARADocument::Listener,
                      private ARAEditorView::Listener
{
public:
    DocumentView (ARAEditorView& editorView, PlayHeadState& playHeadState, AudioProcessorValueTreeState& apvts, AmnesiaDemoAudioProcessor& processor)
        : araEditorView (editorView),
          araDocument (*editorView.getDocumentController()->getDocument<ARADocument>()),
          rulersView (playHeadState, timeToViewScaling, araDocument),
          overlay (playHeadState, timeToViewScaling),
          delayComponent(sectionTree),
          playheadPositionLabel (playHeadState, processor)
    {
        sectionTree = apvts.state.getChildWithName("sections");
        if(sectionTree.isValid())
        {
            sectionIndex = 0;
            sections = Array<Section*>();

            for (ValueTree child : sectionTree)
            {
                addSectionAmnesia(child.getProperty("name"), (float) child.getProperty("startPos"), (float) child.getProperty("endPos"), Colour::fromString(String(child.getProperty("color"))), (int) sectionTree.getChildWithName("delays").getProperty("delay"), (float) (float) sectionTree.getChildWithName("delays").getProperty("beatDelay"), (float) sectionTree.getChildWithName("delays").getProperty("feedback"), (float) sectionTree.getChildWithName("delays").getProperty("mix"), this);
            }
        }

        if (araDocument.getMusicalContexts().size() > 0)
            selectMusicalContext (araDocument.getMusicalContexts().front());

        rulersView.setName("timer");

        addAndMakeVisible (rulersHeader);
        
        viewport.content.addAndMakeVisible (rulersView);

        sectionViewport.onVisibleAreaChanged = [this] (const auto& r)
        {
            resized();
        };

        viewport.onVisibleAreaChanged = [this] (const auto& r)
        {
            viewportHeightOffset = r.getY();
            overlay.setHorizontalOffset (r.getX());
            resized();
        };

        addAndMakeVisible (viewport);
        addAndMakeVisible (overlay);
        addAndMakeVisible (playheadPositionLabel);
        addAndMakeVisible(sectionViewport);

        zoomControls.setZoomInCallback  ([this] { zoom (2.0); });
        zoomControls.setZoomOutCallback ([this] { zoom (0.5); });
        addAndMakeVisible (zoomControls);
        addAndMakeVisible(delayComponent);

        invalidateRegionSequenceViews();

        sectionViewport.setViewedComponent(&sectionList);

        araDocument.addListener (this);
        araEditorView.addListener (this);
    }

    ~DocumentView() override
    {
        araEditorView.removeListener (this);
        araDocument.removeListener (this);
        selectMusicalContext (nullptr);
    }

    //==============================================================================
    // ARADocument::Listener overrides
    void didAddMusicalContextToDocument (ARADocument*, ARAMusicalContext* musicalContext) override
    {
        if (selectedMusicalContext == nullptr)
            selectMusicalContext (musicalContext);
    }

    void willDestroyMusicalContext (ARAMusicalContext* musicalContext) override
    {
        if (selectedMusicalContext == musicalContext)
            selectMusicalContext (nullptr);
    }

    void didReorderRegionSequencesInDocument (ARADocument*) override
    {
        invalidateRegionSequenceViews();
    }

    void didAddRegionSequenceToDocument (ARADocument*, ARARegionSequence*) override
    {
        invalidateRegionSequenceViews();
    }

    void willRemoveRegionSequenceFromDocument (ARADocument*, ARARegionSequence* regionSequence) override
    {
        removeRegionSequenceView (regionSequence);
    }

    void didEndEditing (ARADocument*) override
    {
        rebuildRegionSequenceViews();
        update();
    }

    //==============================================================================
    void changeListenerCallback (ChangeBroadcaster*) override
    {
        update();
    }

    //==============================================================================
    // ARAEditorView::Listener overrides
    void onNewSelection (const ARAViewSelection& viewSelection) override
    {
        auto getNewSelectedMusicalContext = [&viewSelection]() -> ARAMusicalContext*
        {
            if (! viewSelection.getRegionSequences().empty())
                return viewSelection.getRegionSequences<ARARegionSequence>().front()->getMusicalContext();
            else if (! viewSelection.getPlaybackRegions().empty())
                return viewSelection.getPlaybackRegions<ARAPlaybackRegion>().front()->getRegionSequence()->getMusicalContext();

            return nullptr;
        };

        if (auto* newSelectedMusicalContext = getNewSelectedMusicalContext())
            if (newSelectedMusicalContext != selectedMusicalContext)
                selectMusicalContext (newSelectedMusicalContext);

        if (const auto timeRange = viewSelection.getTimeRange())
            overlay.setSelectedTimeRange (*timeRange);
        else
            overlay.setSelectedTimeRange (std::nullopt);
    }

    void onHideRegionSequences (const std::vector<ARARegionSequence*>& regionSequences) override
    {
        hiddenRegionSequences = regionSequences;
        invalidateRegionSequenceViews();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).darker());
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        FlexBox fb;
        fb.justifyContent = FlexBox::JustifyContent::flexStart;
        fb.items.add (FlexItem (playheadPositionLabel).withWidth (headerWidth));
        fb.items.add (FlexItem (sectionViewport).withWidth (380.0f));
        fb.items.add (FlexItem (delayComponent).withWidth (380.f).withMargin ({ 0, 5, 0, 0 }));
        fb.items.add (FlexItem (zoomControls).withMinWidth (80.0f));
        fb.performLayout (bounds.removeFromBottom (200));

        auto headerBounds = bounds.removeFromLeft (headerWidth);
        rulersHeader.setBounds (headerBounds.removeFromTop (20));
        layOutVertically (headerBounds, trackHeaders, viewportHeightOffset);

        viewport.setBounds (bounds);

        overlay.setBounds (bounds.reduced (1));

        const auto width = jmax (timeToViewScaling.getXForTime (timelineLength), viewport.getWidth());
        const auto height = (int) (regionSequenceViews.size() + 1) * trackHeight;
        viewport.content.setSize (width, height);
        viewport.content.resized();
        sectionList.setSize(380.0f, 200);
        sectionList.resized();
    }

    //==============================================================================
    static constexpr int headerWidth = 120;

private:
    struct RegionSequenceViewKey
    {
        explicit RegionSequenceViewKey (ARARegionSequence* regionSequence)
            : orderIndex (regionSequence->getOrderIndex()), sequence (regionSequence)
        {
        }

        bool operator< (const RegionSequenceViewKey& other) const
        {
            return std::tie (orderIndex, sequence) < std::tie (other.orderIndex, other.sequence);
        }

        ARA::ARAInt32 orderIndex;
        ARARegionSequence* sequence;
    };

    void selectMusicalContext (ARAMusicalContext* newSelectedMusicalContext)
    {
        if (auto oldContext = std::exchange (selectedMusicalContext, newSelectedMusicalContext);
            oldContext != selectedMusicalContext)
        {
            if (oldContext != nullptr)
                oldContext->removeListener (this);

            if (selectedMusicalContext != nullptr)
                selectedMusicalContext->addListener (this);
        }
    }

    void zoom (double factor)
    {
        timeToViewScaling.zoom (factor);
        update();
    }

    template <typename T>
    void layOutVertically (Rectangle<int> bounds, T& components, int verticalOffset = 0)
    {
        bounds = bounds.withY (bounds.getY() - verticalOffset).withHeight (bounds.getHeight() + verticalOffset);

        for (auto& component : components)
        {
            component.second->setBounds (bounds.removeFromTop (trackHeight));
            component.second->resized();
        }
    }

    void update()
    {
        timelineLength = 0.0;

        for (const auto& view : regionSequenceViews)
            timelineLength = std::max (timelineLength, view.second->getPlaybackDuration());

        resized();
    }

    void addTrackViews (ARARegionSequence* regionSequence)
    {
        const auto insertIntoMap = [](auto& map, auto key, auto value) -> auto&
        {
            auto it = map.insert ({ std::move (key), std::move (value) });
            return *(it.first->second);
        };

        auto& regionSequenceView = insertIntoMap (
            regionSequenceViews,
            RegionSequenceViewKey { regionSequence },
            std::make_unique<RegionSequenceView> (sectionList, araEditorView, timeToViewScaling, *regionSequence, waveformCache, sectionTree));

        regionSequenceView.addChangeListener (this);
        viewport.content.addAndMakeVisible (regionSequenceView);

        auto& trackHeader = insertIntoMap (trackHeaders,
                                           RegionSequenceViewKey { regionSequence },
                                           std::make_unique<TrackHeader> (araEditorView, *regionSequence));

        addAndMakeVisible (trackHeader);
    }

    void removeRegionSequenceView (ARARegionSequence* regionSequence)
    {
        const auto& view = regionSequenceViews.find (RegionSequenceViewKey { regionSequence });

        if (view != regionSequenceViews.cend())
        {
            removeChildComponent (view->second.get());
            regionSequenceViews.erase (view);
        }

        invalidateRegionSequenceViews();
    }

    void invalidateRegionSequenceViews()
    {
        regionSequenceViewsAreValid = false;
        rebuildRegionSequenceViews();
    }

    void rebuildRegionSequenceViews()
    {
        if (! regionSequenceViewsAreValid && ! araDocument.getDocumentController()->isHostEditingDocument())
        {
            for (auto& view : regionSequenceViews)
                removeChildComponent (view.second.get());

            regionSequenceViews.clear();

            for (auto& view : trackHeaders)
                removeChildComponent (view.second.get());

            trackHeaders.clear();

            for (auto* regionSequence : araDocument.getRegionSequences())
                if (std::find (hiddenRegionSequences.begin(), hiddenRegionSequences.end(), regionSequence) == hiddenRegionSequences.end())
                    addTrackViews (regionSequence);

            update();

            regionSequenceViewsAreValid = true;
        }
    }

    ARAEditorView& araEditorView;
    ARADocument& araDocument;

    bool regionSequenceViewsAreValid = false;

    TimeToViewScaling timeToViewScaling;
    double timelineLength = 0.0;

    ARAMusicalContext* selectedMusicalContext = nullptr;

    std::vector<ARARegionSequence*> hiddenRegionSequences;

    WaveformCache waveformCache;
    std::map<RegionSequenceViewKey, std::unique_ptr<TrackHeader>> trackHeaders;
    std::map<RegionSequenceViewKey, std::unique_ptr<RegionSequenceView>> regionSequenceViews;

    RulersHeader rulersHeader;
    RulersView rulersView;

    VerticalLayoutViewport viewport;
    SectionLayoutViewport sectionViewport;

    OverlayComponent overlay;
    TooltipWindow tooltip;

    ZoomControls zoomControls;
    DelayComponent delayComponent;
    PlayheadPositionLabel playheadPositionLabel;
    SectionList sectionList;

    ValueTree sectionTree;
    int viewportHeightOffset = 0;
};
