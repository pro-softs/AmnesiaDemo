/*
  ==============================================================================

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum class generatorWaveform { kTriangle, kSin, kSaw };
const double kPi = 3.14159;
inline double unipolarToBipolar(double value)
{
    return 2.0*value - 1.0;
}
struct OscillatorParameters
{
    OscillatorParameters() {}
    /** all FXObjects parameter objects require overloaded= operator so remember to add new entries if you add new variables. */
    OscillatorParameters& operator=(const OscillatorParameters& params)    // need this override for collections to work
    {
        if (this == &params)
            return *this;

        waveform = params.waveform;
        frequency_Hz = params.frequency_Hz;
        return *this;
    }

    // --- individual parameters
    generatorWaveform waveform = generatorWaveform::kTriangle; ///< the current waveform
    double frequency_Hz = 0.0;    ///< oscillator frequency
};

struct SignalGenData
{
    SignalGenData() {}

    double normalOutput = 0.0;            ///< normal
    double invertedOutput = 0.0;        ///< inverted
    double quadPhaseOutput_pos = 0.0;    ///< 90 degrees out
    double quadPhaseOutput_neg = 0.0;    ///< -90 degrees out
};

inline double doLinearInterpolation(double y1, double y2, double fractional_X)
{
    // --- check invalid condition
    if (fractional_X >= 1.0) return y2;

    // --- use weighted sum method of interpolating
    return fractional_X*y2 + (1.0 - fractional_X)*y1;
}

class IAudioSignalGenerator
{
public:
    // --- pure virtual, derived classes must implement or will not compile
    //     also means this is a pure abstract base class and is incomplete,
    //     so it can only be used as a base class
    //
    /** Sample rate may or may not be required, but usually is */
    virtual bool reset(double _sampleRate) = 0;

    /** render the generator output */
    virtual const SignalGenData renderAudioOutput() = 0;
};

class LFO : public IAudioSignalGenerator
{
public:
    LFO() {    srand(time(NULL)); }    /* C-TOR */
    virtual ~LFO() {}                /* D-TOR */

    /** reset members to initialized state */
    virtual bool reset(double _sampleRate)
    {
        sampleRate = _sampleRate;
        phaseInc = lfoParameters.frequency_Hz / sampleRate;

        // --- timebase variables
        modCounter = 0.0;            ///< modulo counter [0.0, +1.0]
        modCounterQP = 0.25;        ///<Quad Phase modulo counter [0.0, +1.0]

        return true;
    }

    /** get parameters: note use of custom structure for passing param data */
    /**
    \return OscillatorParameters custom data structure
    */
    OscillatorParameters getParameters(){ return lfoParameters; }

    /** set parameters: note use of custom structure for passing param data */
    /**
    \param OscillatorParameters custom data structure
    */
    void setParameters(const OscillatorParameters& params)
    {
        if(params.frequency_Hz != lfoParameters.frequency_Hz)
            // --- update phase inc based on osc freq and fs
            phaseInc = params.frequency_Hz / sampleRate;

        lfoParameters = params;
    }

    /** render a new audio output structure */
    const SignalGenData renderAudioOutput() {
        checkAndWrapModulo(modCounter, phaseInc);

        // --- QP output always follows location of current modulo; first set equal
        modCounterQP = modCounter;

        // --- then, advance modulo by quadPhaseInc = 0.25 = 90 degrees, AND wrap if needed
        advanceAndCheckWrapModulo(modCounterQP, 0.25);

        SignalGenData output;
        generatorWaveform waveform = lfoParameters.waveform;

        // --- calculate the oscillator value
        if (waveform == generatorWaveform::kSin)
        {
            // --- calculate normal angle
            double angle = modCounter*2.0*kPi - kPi;

            // --- norm output with parabolicSine approximation
            output.normalOutput = parabolicSine(-angle);

            // --- calculate QP angle
            angle = modCounterQP*2.0*kPi - kPi;

            // --- calc QP output
            output.quadPhaseOutput_pos = parabolicSine(-angle);
        }
        else if (waveform == generatorWaveform::kTriangle)
        {
            // triv saw
            output.normalOutput = unipolarToBipolar(modCounter);

            // bipolar triagle
            output.normalOutput = 2.0*fabs(output.normalOutput) - 1.0;

            // -- quad phase
            output.quadPhaseOutput_pos = unipolarToBipolar(modCounterQP);

            // bipolar triagle
            output.quadPhaseOutput_pos = 2.0*fabs(output.quadPhaseOutput_pos) - 1.0;
        }
        else if (waveform == generatorWaveform::kSaw)
        {
            output.normalOutput = unipolarToBipolar(modCounter);
            output.quadPhaseOutput_pos = unipolarToBipolar(modCounterQP);
        }

        // --- invert two main outputs to make the opposite versions
        output.quadPhaseOutput_neg = -output.quadPhaseOutput_pos;
        output.invertedOutput = -output.normalOutput;

        // --- setup for next sample period
        advanceModulo(modCounter, phaseInc);

        return output;
    }

protected:
    // --- parameters
    OscillatorParameters lfoParameters; ///< obejcgt parameters

    // --- sample rate
    double sampleRate = 0.0;            ///< sample rate

    // --- timebase variables
    double modCounter = 0.0;            ///< modulo counter [0.0, +1.0]
    double phaseInc = 0.0;                ///< phase inc = fo/fs
    double modCounterQP = 0.25;            ///<Quad Phase modulo counter [0.0, +1.0]

    /** check the modulo counter and wrap if needed */
    inline bool checkAndWrapModulo(double& moduloCounter, double phaseInc)
    {
        // --- for positive frequencies
        if (phaseInc > 0 && moduloCounter >= 1.0)
        {
            moduloCounter -= 1.0;
            return true;
        }

        // --- for negative frequencies
        if (phaseInc < 0 && moduloCounter <= 0.0)
        {
            moduloCounter += 1.0;
            return true;
        }

        return false;
    }

    /** advanvce the modulo counter, then check the modulo counter and wrap if needed */
    inline bool advanceAndCheckWrapModulo(double& moduloCounter, double phaseInc)
    {
        // --- advance counter
        moduloCounter += phaseInc;

        // --- for positive frequencies
        if (phaseInc > 0 && moduloCounter >= 1.0)
        {
            moduloCounter -= 1.0;
            return true;
        }

        // --- for negative frequencies
        if (phaseInc < 0 && moduloCounter <= 0.0)
        {
            moduloCounter += 1.0;
            return true;
        }

        return false;
    }

    /** advanvce the modulo counter */
    inline void advanceModulo(double& moduloCounter, double phaseInc) { moduloCounter += phaseInc; }

    const double B = 4.0 / kPi;
    const double C = -4.0 / (kPi* kPi);
    const double P = 0.225;
    /** parabolic sinusoidal calcualtion; NOTE: input is -pi to +pi http://devmaster.net/posts/9648/fast-and-accurate-sine-cosine */
    inline double parabolicSine(double angle)
    {
        double y = B * angle + C * angle * fabs(angle);
        y = P * (y * fabs(y) - y) + y;
        return y;
    }
};

template <typename T>
class CircularBuffer
{
public:
    CircularBuffer() {}        /* C-TOR */
    ~CircularBuffer() {}    /* D-TOR */

    /** flush buffer by resetting all values to 0.0 */
    void flushBuffer(){ memset(&buffer[0], 0, bufferLength * sizeof(T)); }

    /** Create a buffer based on a target maximum in SAMPLES
    //       do NOT call from realtime audio thread; do this prior to any processing */
    void createCircularBuffer(unsigned int _bufferLength)
    {
        // --- find nearest power of 2 for buffer, and create
        createCircularBufferPowerOfTwo((unsigned int)(pow(2, ceil(log(_bufferLength) / log(2)))));
    }

    /** Create a buffer based on a target maximum in SAMPLESwhere the size is
        pre-calculated as a power of two */
    void createCircularBufferPowerOfTwo(unsigned int _bufferLengthPowerOfTwo)
    {
        // --- reset to top
        writeIndex = 0;

        // --- find nearest power of 2 for buffer, save it as bufferLength
        bufferLength = _bufferLengthPowerOfTwo;

        // --- save (bufferLength - 1) for use as wrapping mask
        wrapMask = bufferLength - 1;

        // --- create new buffer
        buffer.reset(new T[bufferLength]);

        // --- flush buffer
        flushBuffer();
    }

    /** write a value into the buffer; this overwrites the previous oldest value in the buffer */
    void writeBuffer(T input)
    {
        // --- write and increment index counter
        buffer[writeIndex++] = input;

        // --- wrap if index > bufferlength - 1
        writeIndex &= wrapMask;
    }

    /** read an arbitrary location that is delayInSamples old */
    T readBuffer(int delayInSamples)//, bool readBeforeWrite = true)
    {
        // --- subtract to make read index
        //     note: -1 here is because we read-before-write,
        //           so the *last* write location is what we use for the calculation
        int readIndex = (writeIndex - 1) - delayInSamples;

        // --- autowrap index
        readIndex &= wrapMask;

        // --- read it
        return buffer[readIndex];
    }

    /** read an arbitrary location that includes a fractional sample */
    T readBuffer(double delayInFractionalSamples)
    {
        // --- truncate delayInFractionalSamples and read the int part
        T y1 = readBuffer((int)delayInFractionalSamples);

        // --- if no interpolation, just return value
        if (!interpolate) return y1;

        // --- else do interpolation
        //
        // --- read the sample at n+1 (one sample OLDER)
        T y2 = readBuffer((int)delayInFractionalSamples + 1);

        // --- get fractional part
        double fraction = delayInFractionalSamples - (int)delayInFractionalSamples;

        // --- do the interpolation (you could try different types here)
        return doLinearInterpolation(y1, y2, fraction);
    }

    /** enable or disable interpolation; usually used for diagnostics or in algorithms that require strict integer samples times */
    void setInterpolate(bool b) { interpolate = b; }

private:
    std::unique_ptr<T[]> buffer = nullptr;    ///< smart pointer will auto-delete
    unsigned int writeIndex = 0;        ///> write index
    unsigned int bufferLength = 1024;    ///< must be nearest power of 2
    unsigned int wrapMask = 1023;        ///< must be (bufferLength - 1)
    bool interpolate = true;            ///< interpolation (default is ON)
};

struct AlphaSimpleDelayParameters
{
    AlphaSimpleDelayParameters& operator= (const AlphaSimpleDelayParameters& parameters)
    {
        if (this != &parameters)
        {
            wetDryMix = parameters.wetDryMix;
            feedback = parameters.feedback;
            delay = parameters.delay;
        }

        return *this;
    }

    double wetDryMix = 0.5;
    double feedback = 0.0;
    double delay = 0.5; // in seconds
};

class IAudioSignalProcessor
{
public:
    // --- pure virtual, derived classes must implement or will not compile
    //     also means this is a pure abstract base class and is incomplete,
    //     so it can only be used as a base class
    //
    /** initialize the object with the new sample rate */
    virtual bool reset(double _sampleRate) = 0;

    /** process one sample in and out */
    virtual double processAudioSample(double xn) = 0;

    /** return true if the derived object can process a frame, false otherwise */
    virtual bool canProcessAudioFrame() = 0;

    /** set or change the sample rate; normally this is done during reset( ) but may be needed outside of initialzation */
    virtual void setSampleRate(double _sampleRate) {}

    /** switch to enable/disable the aux input */
    virtual void enableAuxInput(bool enableAuxInput) {}

    /** for processing objects with a sidechain input or other necessary aux input
            the return value is optional and will depend on the subclassed object */
    virtual double processAuxInputAudioSample(double xn)
    {
        // --- do nothing
        return xn;
    }

    /** for processing objects with a sidechain input or other necessary aux input
    --- optional processing function
        e.g. does not make sense for some objects to implement this such as inherently mono objects like Biquad
             BUT a processor that must use both left and right channels (ping-pong delay) would require it */
    virtual bool processAudioFrame(const float* inputFrame,        /* ptr to one frame of data: pInputFrame[0] = left, pInputFrame[1] = right, etc...*/
                                   float* outputFrame,
                                   uint32_t inputChannels,
                                   uint32_t outputChannels)
    {
        // --- do nothing
        return false; // NOT handled
    }
};

class AlphaSimpleDelay : public IAudioSignalProcessor
{
public:
    AlphaSimpleDelay() {}
    ~AlphaSimpleDelay() {}

    virtual bool reset(double _sampleRate) override
    {
        sampleRate = _sampleRate;
        delayInSamples = sampleRate + parameters.delay;
        delayBufferSize = (sampleRate * 2) + 1;

        leftDelayBuffer.createCircularBuffer(delayBufferSize);
        rightDelayBuffer.createCircularBuffer(delayBufferSize);

        return true;
    }

    virtual bool canProcessAudioFrame() override
    {
        return true;
    }

    virtual double processAudioSample(double xn) override
    {
        smoothedDelay -= 0.0005f * (smoothedDelay - parameters.delay);
        delayInSamples = sampleRate * smoothedDelay;

        leftDelayBuffer.writeBuffer(xn + leftChannelFeedback);

        double yn = leftDelayBuffer.readBuffer(delayInSamples);

        leftChannelFeedback = parameters.feedback * yn;

        yn = xn * (1 - parameters.wetDryMix) + (yn * parameters.wetDryMix);

        return yn;
    }

    virtual bool processAudioFrame(const float* inputFrame,
                                   float* outputFrame,
                                   uint32_t inputChannels,
                                   uint32_t outputChannels) override
    {
        if (inputChannels == 0 || outputChannels == 0)
        {
            return false;
        }

        // Mono - use processAudioSample
        if (outputChannels == 1)
        {
            outputFrame[0] = processAudioSample(inputFrame[0]);
            outputFrame[1] = outputFrame[0];
            return true;
        }

        // Stereo processing
        smoothedDelay -= 0.0005f * (smoothedDelay - parameters.delay);
        delayInSamples = sampleRate * smoothedDelay;

        leftDelayBuffer.writeBuffer(inputFrame[0] + leftChannelFeedback);
        rightDelayBuffer.writeBuffer(inputFrame[1] + rightChannelFeedback);

        double leftDelayedSample = leftDelayBuffer.readBuffer(delayInSamples);
        double rightDelayedSample = rightDelayBuffer.readBuffer(delayInSamples);

        leftChannelFeedback = parameters.feedback * leftDelayedSample;
        rightChannelFeedback = parameters.feedback * rightDelayedSample;

        outputFrame[0] = inputFrame[0] * (1 - parameters.wetDryMix) + (leftDelayedSample * parameters.wetDryMix);
        outputFrame[1] = inputFrame[1] * (1 - parameters.wetDryMix) + (rightDelayedSample * parameters.wetDryMix);
        return true;
    }

    AlphaSimpleDelayParameters getParameters()
    {
        return parameters;
    }

    void setParameters(AlphaSimpleDelayParameters _parameters)
    {
        parameters = _parameters;
    }

private:
    AlphaSimpleDelayParameters parameters;
    double sampleRate = 0;
    double delayInSamples = 0;
    double smoothedDelay = 0;
    int delayBufferSize= 0;
    float readHead = 0;
    int writeHead = 0;

    float leftChannelFeedback = 0.0;
    float rightChannelFeedback = 0.0;

    CircularBuffer<float> leftDelayBuffer;
    CircularBuffer<float> rightDelayBuffer;
};


struct AlphaChorusParameters
{
    AlphaChorusParameters& operator= (const AlphaChorusParameters& parameters)
    {
        if (this != &parameters)
        {
            wetDryMix = parameters.wetDryMix;
            feedback = parameters.feedback;
            rate = parameters.rate;
            depth = parameters.depth;
        }

        return *this;
    }

    double wetDryMix = 0.5;
    double feedback = 0.0;
    double rate = 10.0; // Freq. of LFOs in Hz
    double depth = 0.5;
};

class AlphaChorus : public IAudioSignalProcessor
{
public:
    AlphaChorus() {}
    ~AlphaChorus() {}

    virtual bool reset(double _sampleRate) override
    {
        sampleRate = _sampleRate;
        delayBufferSize = (sampleRate * 2) + 1;

        leftDelayBuffer.createCircularBuffer(delayBufferSize);
        rightDelayBuffer.createCircularBuffer(delayBufferSize);

        leftLFO.reset(sampleRate);
        rightLFO.reset(sampleRate);

        auto leftLfoParams = leftLFO.getParameters();
        auto rightLfoParams = rightLFO.getParameters();
        leftLfoParams.waveform = generatorWaveform::kSin;
        rightLfoParams.waveform = generatorWaveform::kSin;

        leftLFO.setParameters(leftLfoParams);
        rightLFO.setParameters(rightLfoParams);

        return true;
    }

    virtual bool canProcessAudioFrame() override
    {
        return true;
    }

    virtual double processAudioSample(double xn) override
    {
        auto leftLfoParams = leftLFO.getParameters();
        leftLfoParams.frequency_Hz = parameters.rate;
        leftLfoParams.waveform = generatorWaveform::kSin;
        leftLFO.setParameters(leftLfoParams);

        auto leftLfoOutput = leftLFO.renderAudioOutput();
        auto leftLfoValue = static_cast<float>(leftLfoOutput.normalOutput * parameters.depth);

        // Chorus Effect delay values
        float leftLfoMapped = juce::jmap(leftLfoValue, -1.0f, 1.0f, 0.005f, 0.030f);
        leftDelayInSamples = sampleRate * leftLfoMapped;

        leftReadHead = writeHead - leftDelayInSamples;

        leftDelayBuffer.writeBuffer(xn + leftChannelFeedback);

        float leftDelayedSample = leftDelayBuffer.readBuffer(leftDelayInSamples);
        leftChannelFeedback = parameters.feedback * leftDelayedSample;

        float yn = xn * (1 - parameters.wetDryMix) + (leftDelayedSample * parameters.wetDryMix);

        return yn;
    }

    virtual bool processAudioFrame(const float* inputFrame,
                                   float* outputFrame,
                                   uint32_t inputChannels,
                                   uint32_t outputChannels) override
    {
        if (inputChannels == 0 || outputChannels == 0)
        {
            return false;
        }

        // Mono - use processAudioSample
        if (outputChannels == 1)
        {
            outputFrame[0] = processAudioSample(inputFrame[0]);
            outputFrame[1] = outputFrame[0];

            return true;
        }

        // Stereo processing
        auto leftLfoParams = leftLFO.getParameters();
        leftLfoParams.frequency_Hz = parameters.rate;
        leftLfoParams.waveform = generatorWaveform::kTriangle;
        leftLFO.setParameters(leftLfoParams);

        auto rightLfoParams = rightLFO.getParameters();
        rightLfoParams.frequency_Hz = parameters.rate;
        rightLfoParams.waveform = generatorWaveform::kTriangle;
        rightLFO.setParameters(rightLfoParams);

        auto leftLfoOutput = leftLFO.renderAudioOutput();
        auto leftLfoValue = static_cast<float>(leftLfoOutput.normalOutput * parameters.depth);

        auto rightLfoOutput = rightLFO.renderAudioOutput();
        auto rightLfoValue = static_cast<float>(rightLfoOutput.normalOutput * parameters.depth);

        // Chorus Effect delay values
        float leftLfoMapped = juce::jmap(leftLfoValue, -1.0f, 1.0f, 0.005f, 0.030f);
        leftDelayInSamples = sampleRate * leftLfoMapped;
        float rightLfoMapped = juce::jmap(rightLfoValue, -1.0f, 1.0f, 0.005f, 0.030f);
        rightDelayInSamples = sampleRate * rightLfoMapped;

        leftReadHead = writeHead - leftDelayInSamples;
        rightReadHead = writeHead - rightDelayInSamples;

        leftDelayBuffer.writeBuffer(inputFrame[0] + leftChannelFeedback);
        rightDelayBuffer.writeBuffer(inputFrame[1] + rightChannelFeedback);

        float leftDelayedSample = leftDelayBuffer.readBuffer(leftDelayInSamples);
        leftChannelFeedback = parameters.feedback * leftDelayedSample;
        float rightDelayedSample = rightDelayBuffer.readBuffer(rightDelayInSamples);
        rightChannelFeedback = parameters.feedback * rightDelayedSample;

        outputFrame[0] = inputFrame[0] * (1 - parameters.wetDryMix) + (leftDelayedSample * parameters.wetDryMix);
        outputFrame[1] = inputFrame[1] * (1 - parameters.wetDryMix) + (rightDelayedSample * parameters.wetDryMix);
        return true;
    }

    AlphaChorusParameters getParameters()
    {
        return parameters;
    }

    void setParameters(AlphaChorusParameters _parameters)
    {
        parameters = _parameters;
    }

private:
    AlphaChorusParameters parameters;
    double sampleRate = 0;
    double leftDelayInSamples = 0;
    double rightDelayInSamples = 0;
    int delayBufferSize = 0;
    float leftReadHead = 0;
    float rightReadHead = 0;
    int writeHead = 0;

    float leftChannelFeedback = 0.0;
    float rightChannelFeedback = 0.0;

    CircularBuffer<float> leftDelayBuffer;
    CircularBuffer<float> rightDelayBuffer;

    LFO leftLFO;
    LFO rightLFO;
};
