/**********************************************************************

  Rustacity: A Digital Audio Editor

  Wahwah

  Effect programming:
  Nasca Octavian Paul (Paul Nasca)

  UI programming:
  Dominic Mazzoni (with the help of wxDesigner)
  Vaughan Johnson (Preview)

**********************************************************************/

#ifndef __RUSTACITY_EFFECT_WAHWAH__
#define __RUSTACITY_EFFECT_WAHWAH__

#include "Effect.h"

/// The wah filter lives in the Rust DSP core (rust/dsp); this is an opaque
/// handle to it.  See rust/dsp/include/rustacity_dsp.h for the C ABI.
struct rustacity_wahwah;

class wxSlider;
class wxTextCtrl;
class ShuttleGui;

class EffectWahwah final : public Effect
{
public:
   static const ComponentInterfaceSymbol Symbol;

   EffectWahwah();
   virtual ~EffectWahwah();

   // ComponentInterface implementation

   ComponentInterfaceSymbol GetSymbol() override;
   TranslatableString GetDescription() override;
   wxString ManualPage() override;

   // EffectDefinitionInterface implementation

   EffectType GetType() override;
   bool SupportsRealtime() override;

   // EffectClientInterface implementation

   unsigned GetAudioInCount() override;
   unsigned GetAudioOutCount() override;
   bool ProcessInitialize(sampleCount totalLen, ChannelNames chanMap = NULL) override;
   size_t ProcessBlock(float **inBlock, float **outBlock, size_t blockLen) override;
   bool ProcessFinalize() override;
   bool RealtimeInitialize() override;
   bool RealtimeAddProcessor(unsigned numChannels, float sampleRate) override;
   bool RealtimeFinalize() override;
   size_t RealtimeProcess(int group,
                                       float **inbuf,
                                       float **outbuf,
                                       size_t numSamples) override;
   bool DefineParams( ShuttleParams & S ) override;
   bool GetAutomationParameters(CommandParameters & parms) override;
   bool SetAutomationParameters(CommandParameters & parms) override;

   // Effect implementation

   void PopulateOrExchange(ShuttleGui & S) override;
   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;

private:
   // EffectWahwah implementation

   void OnFreqSlider(wxCommandEvent & evt);
   void OnPhaseSlider(wxCommandEvent & evt);
   void OnDepthSlider(wxCommandEvent & evt);
   void OnResonanceSlider(wxCommandEvent & evt);
   void OnFreqOffSlider(wxCommandEvent & evt);
   void OnGainSlider(wxCommandEvent & evt);

   void OnFreqText(wxCommandEvent & evt);
   void OnPhaseText(wxCommandEvent & evt);
   void OnDepthText(wxCommandEvent & evt);
   void OnResonanceText(wxCommandEvent & evt);
   void OnFreqOffText(wxCommandEvent & evt);
   void OnGainText(wxCommandEvent & evt);

private:
   rustacity_wahwah* mMaster;
   std::vector<rustacity_wahwah*> mSlaves;

   /* Parameters:
   mFreq - LFO frequency
   mPhase - LFO startphase in RADIANS - useful for stereo WahWah
   mDepth - Wah depth
   mRes - Resonance
   mFreqOfs - Wah frequency offset
   mOutGain - output gain

   !!!!!!!!!!!!! IMPORTANT!!!!!!!!! :
   mDepth and mFreqOfs should be from 0(min) to 1(max) !
   mRes should be greater than 0 !  */

   double mFreq;
   double mPhase;
   int mDepth;
   double mRes;
   int mFreqOfs;
   double mOutGain;

   wxTextCtrl *mFreqT;
   wxTextCtrl *mPhaseT;
   wxTextCtrl *mDepthT;
   wxTextCtrl *mResT;
   wxTextCtrl *mFreqOfsT;
   wxTextCtrl *mOutGainT;

   wxSlider *mFreqS;
   wxSlider *mPhaseS;
   wxSlider *mDepthS;
   wxSlider *mResS;
   wxSlider *mFreqOfsS;
   wxSlider *mOutGainS;

   DECLARE_EVENT_TABLE()
};

#endif

