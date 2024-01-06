/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::string GateProcessor::paramsNames[] = { "Threshold", "Attack", "Release", "Mix", "Volume" };

//==============================================================================
GateProcessor::GateProcessor()
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
	thresholdParameter = apvts.getRawParameterValue(paramsNames[0]);
	attackParameter    = apvts.getRawParameterValue(paramsNames[1]);
	releaseParameter   = apvts.getRawParameterValue(paramsNames[2]);
	mixParameter       = apvts.getRawParameterValue(paramsNames[3]);
	volumeParameter    = apvts.getRawParameterValue(paramsNames[4]);
}

GateProcessor::~GateProcessor()
{
}

//==============================================================================
const juce::String GateProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GateProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool GateProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool GateProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double GateProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GateProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int GateProcessor::getCurrentProgram()
{
    return 0;
}

void GateProcessor::setCurrentProgram (int index)
{
}

const juce::String GateProcessor::getProgramName (int index)
{
    return {};
}

void GateProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void GateProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void GateProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	// Get params
	const float thresholdGain = juce::Decibels::decibelsToGain(thresholdParameter->load());
	const float attack = attackParameter->load();
	const float release = releaseParameter->load();
	const float mix = mixParameter->load();
	const float volume = juce::Decibels::decibelsToGain(volumeParameter->load());

	// Mics constants
	const float mixInverse = 1.0f - mix;
	const int channels = getTotalNumOutputChannels();
	const int samples = buffer.getNumSamples();

	for (int channel = 0; channel < channels; ++channel)
	{
		auto* channelBuffer = buffer.getWritePointer(channel);
		auto& envelopeFollower = m_envelopeFollower[channel];
		envelopeFollower.setCoef(attack, release);

		for (int sample = 0; sample < samples; ++sample)
		{
			// Get input
			const float in = channelBuffer[sample];

			float gateVolume = envelopeFollower.process((float)(fabsf(in) > thresholdGain));

			// Apply volume
			channelBuffer[sample] = volume * ((mix * gateVolume + mixInverse) * in);
		}
	}
}

//==============================================================================
bool GateProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GateProcessor::createEditor()
{
    return new GateProcessorEditor(*this, apvts);
}

//==============================================================================
void GateProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void GateProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(apvts.state.getType()))
			apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout GateProcessor::createParameterLayout()
{
	APVTS::ParameterLayout layout;

	using namespace juce;

	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[0], paramsNames[0], NormalisableRange<float>(-60.0f,   0.0f,  1.0f, 1.0f), -12.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[1], paramsNames[1], NormalisableRange<float>(  0.0f, 400.0f, 0.05f, 0.3f),  10.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[2], paramsNames[2], NormalisableRange<float>(  0.0f, 400.0f, 0.05f, 0.3f), 100.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[3], paramsNames[3], NormalisableRange<float>(  0.0f,   1.0f, 0.05f, 1.0f),   1.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[4], paramsNames[4], NormalisableRange<float>(-12.0f,  12.0f,  0.1f, 1.0f),   0.0f));

	return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GateProcessor();
}
