/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
EqualizerAudioProcessor::EqualizerAudioProcessor()
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
}

EqualizerAudioProcessor::~EqualizerAudioProcessor()
{
}

//==============================================================================
const juce::String EqualizerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EqualizerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EqualizerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EqualizerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EqualizerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EqualizerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EqualizerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EqualizerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EqualizerAudioProcessor::getProgramName (int index)
{
    return {};
}

void EqualizerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EqualizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.numChannels = 1;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    auto chainSettings = getChainSettings(apvts);
    updatePeakFilter(chainSettings);

    auto lowCutCoefficient = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.lowCutFreq,
        sampleRate,
        2 * chainSettings.lowCutSlope + 1);

    auto& leftLowCut = leftChain.get<chainPosition::LowCut>();

    updateCutFilter(leftLowCut, lowCutCoefficient, chainSettings.lowCutSlope);

    auto& rightLowCut = rightChain.get<chainPosition::LowCut>();

    updateCutFilter(rightLowCut, lowCutCoefficient, chainSettings.lowCutSlope);

    auto highCutCoefficient = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.highCutFreq,
        sampleRate,
        2 * chainSettings.highCutSlope + 1);

    auto& leftHighCut = leftChain.get<chainPosition::HighCut>();
    auto& rightHighCut = rightChain.get<chainPosition::HighCut>();

    updateCutFilter(leftHighCut, highCutCoefficient, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficient, chainSettings.highCutSlope);
}

void EqualizerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EqualizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void EqualizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto chainSettings = getChainSettings(apvts);
    updatePeakFilter(chainSettings);
     
    auto lowcutCoefficient = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.lowCutFreq,
        getSampleRate(),
        2 * chainSettings.lowCutSlope + 1 );

    auto& leftLowCut = leftChain.get<chainPosition::LowCut>();

    updateCutFilter(leftLowCut, lowcutCoefficient, chainSettings.lowCutSlope);

    auto& rightLowCut = rightChain.get<chainPosition::LowCut>();
    updateCutFilter(rightLowCut, lowcutCoefficient, chainSettings.lowCutSlope);
    
    auto highCutCoefficient = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
        chainSettings.highCutFreq,
        getSampleRate(),
        2 * chainSettings.highCutSlope + 1);

    auto& leftHighCut = leftChain.get<chainPosition::HighCut>();
    auto& rightHighCut = rightChain.get<chainPosition::HighCut>();

    updateCutFilter(leftHighCut, highCutCoefficient, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficient, chainSettings.highCutSlope);

    juce::dsp::AudioBlock<float> block (buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool EqualizerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EqualizerAudioProcessor::createEditor()
{
    /*return new EqualizerAudioProcessorEditor (*this);*/
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void EqualizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EqualizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings Settings;

    Settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();

    Settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();

    Settings.peakFreq =  apvts.getRawParameterValue("Peak Freq")->load();

    Settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();

    Settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();

    Settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());

    Settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());



    return Settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout EqualizerAudioProcessor::createParameterLayout()
{
   // return juce::AudioProcessorValueTreeState::ParameterLayout();

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>
        (20.f, 20000.f, 1.f, .25f), 20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>
        (20.f, 20000.f, 1.f, .25f), 20000.f));


    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>
        (20.f, 20000.f, 1.f, .25f), 750.f));


    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>
        (-24.f, 24.f, .5f, 1.f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality", "Peak Quality", juce::NormalisableRange<float>
        (.1f, 10.f, .05f, 1.f), 1.f));

    juce::StringArray stringArray;
    for (int i{ 0 }; i < 4; ++i) {
        juce::String str;
        str << (12 + i * 12);
        str << " db/Oct";
        stringArray.add(str);
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>
        ("LowCut Slope", "LowCut Slope", stringArray, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>
        ("HighCut Slope", "HighCut Slope", stringArray, 0));


    return layout;
}

void EqualizerAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings)
{
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), chainSettings.peakFreq, chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    /**leftChain.get<chainPosition::Peak>().coefficients = *peakCoefficients;
    *rightChain.get < chainPosition::Peak>().coefficients = *peakCoefficients;*/
    updateCoefficients(leftChain.get<chainPosition::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<chainPosition::Peak>().coefficients, peakCoefficients);
}

void EqualizerAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqualizerAudioProcessor();
}


