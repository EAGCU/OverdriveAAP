/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OverdriveAAPAudioProcessor::OverdriveAAPAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // inputGain parameter. This is used in processBlock to scale the input signal
    inputGain = new juce::AudioParameterFloat("inputGain", "Input Gain", 0.0f, 10.0f, 1.0f);
    addParameter(inputGain);

    // add dry wet slider
    dryWetMix = new juce::AudioParameterFloat("dryWetMix", "Dry / Wet Mix", 0.0f, 1.0f, 0.5f);
    addParameter(dryWetMix);

}

OverdriveAAPAudioProcessor::~OverdriveAAPAudioProcessor()
{
}

//==============================================================================
const juce::String OverdriveAAPAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool OverdriveAAPAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool OverdriveAAPAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool OverdriveAAPAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double OverdriveAAPAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int OverdriveAAPAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int OverdriveAAPAudioProcessor::getCurrentProgram()
{
    return 0;
}

void OverdriveAAPAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String OverdriveAAPAudioProcessor::getProgramName (int index)
{
    return {};
}

void OverdriveAAPAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void OverdriveAAPAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    dryWetMixer.prepare(spec);
    // set up mixing rule
    dryWetMixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);

}

void OverdriveAAPAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OverdriveAAPAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void OverdriveAAPAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

        // == DRY/WET ==
        // Create an AudioBlock for the dry samples 
        juce::dsp::AudioBlock<float> dryBlock(buffer);

        // == DRY/WET ==
        // push in dry samples to our dryWetMixerObject
        dryWetMixer.pushDrySamples(dryBlock);

        // == DRY/WET ==
        // get dry wet mix from slider and set mix proprtion
        float mix = dryWetMix->get();
        dryWetMixer.setWetMixProportion(mix); 

    // Audio processing loop
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // stores output sample we compute below
        float out = 0.0f;

        // set the thresholds
        float thresh1 = 1.0f / 3.0f; // 0.33 recurring
        float thresh2 = 2.0f / 3.0f; // 0.66 recurring

        // loop round each sample in the buffer
        for (int i = 0; i < buffer.getNumSamples(); i++) 
        {
            // multiply our input data by the value in out inputGain parameter
            float in = channelData[i] * inputGain->get();
            
            // use signum function to detect and store the sign
            // see function directly below processBlock for the definition
            int sign = signum(in);

            // get absolute value ('fabs' function means 'Float ABSolute value')
            float absIn = fabs(in);

            // if statements check the conditions
            if (absIn > thresh2)
            {
                // if over 0.66, assign value 0.99 to out. As noted in the lecture, if set to 1 it may clip
                out = 0.99f * sign;
            }
            else if (absIn > thresh1)
            {
                // if over 0.33, 'out' becomes the result of...
                out = ((3.0f - (2.0f - 3.0f * absIn) * (2.0f - 3.0f * absIn)) / 3.0f) * sign;
            }
            else
            {
                // if between 0-0.33, multiply the input signal by 2
                // note we're using the original input signal here as it's just multiplication
                // ... no need to use the 'sign' variable as we have above
                out = 2.0f * in;

            }

            // Finally, assign 'out' to the position in the buffer
            channelData[i] = out;        
        }
    }


        // == DRY/WET ==
        // at this poinnt, the buffer has been overwritten by 'wet' samples
        // therefore, we can pass that in to mix the wet samples with the dry
        dryWetMixer.mixWetSamples(buffer);
    
}

// This function takes the signal as input, then returns 1, 0, or -1 to indicate the sign 
int OverdriveAAPAudioProcessor::signum(float x)
{
    if (x > 0) // if over 0, return 1 to indicate the signal is positive
        return 1;

    else if (x < 0) // if less than zero, return -1 to indicate the signal is negative
        return -1;
   
    else // else return zero
        return 0;

}


//==============================================================================
bool OverdriveAAPAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* OverdriveAAPAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void OverdriveAAPAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void OverdriveAAPAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OverdriveAAPAudioProcessor();
}
