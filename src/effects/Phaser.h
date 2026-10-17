/**********************************************************************

  Rustacity: A Digital Audio Editor

  Phaser

  Effect programming:
  Nasca Octavian Paul (Paul Nasca)

  UI programming:
  Dominic Mazzoni (with the help of wxDesigner)
  Vaughan Johnson (Preview)

**********************************************************************/

#ifndef __RUSTACITY_EFFECT_PHASER__
#define __RUSTACITY_EFFECT_PHASER__

#include "Effect.h"

/// The allpass filter lives in the Rust DSP core (rust/dsp); this is an
/// opaque handle to it.  See rust/dsp/include/rustacity_dsp.h for the C ABI.
struct rustacity_phaser;

class wxSlider;
class wxTextCtrl;
class ShuttleGui;

// The most stages the effect offers; the Rust filter clamps a block to the
// same maximum (MAX_STAGES in rust/dsp/src/phaser.rs).
#define NUM_STAGES 24

class EffectPhaser final : public Effect
{
public:
   static const ComponentInterfaceSymbol Symbol;

   EffectPhaser();
   virtual ~EffectPhaser();

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
   // EffectPhaser implementation

   void OnStagesSlider(wxCommandEvent & evt);
   void OnDryWetSlider(wxCommandEvent & evt);
   void OnFeedbackSlider(wxCommandEvent & evt);
   void OnDepthSlider(wxCommandEvent & evt);
   void OnPhaseSlider(wxCommandEvent & evt);
   void OnFreqSlider(wxCommandEvent & evt);
   void OnGainSlider(wxCommandEvent & evt);

   void OnStagesText(wxCommandEvent & evt);
   void OnDryWetText(wxCommandEvent & evt);
   void OnFeedbackText(wxCommandEvent & evt);
   void OnDepthText(wxCommandEvent & evt);
   void OnPhaseText(wxCommandEvent & evt);
   void OnFreqText(wxCommandEvent & evt);
   void OnGainText(wxCommandEvent & evt);
/*
    Phaser Parameters

 mFreq       - Phaser's LFO frequency
 mPhase      - Phaser's LFO startphase (radians), needed for stereo Phasers
 mDepth      - Phaser depth (0 - no depth, 255 - max depth)
 mStages     - Phaser stages (recomanded from 2 to 16-24, and EVEN NUMBER)
 mDryWet     - Dry/wet mix, (0 - dry, 128 - dry=wet, 255 - wet)
 mFeedback   - Phaser FeedBack (0 - no feedback, 100 = 100% Feedback,
                               -100 = -100% FeedBack)
*/

private:
   rustacity_phaser* mMaster;
   std::vector<rustacity_phaser*> mSlaves;

   // parameters
   int mStages;
   int mDryWet;
   double mFreq;
   double mPhase;
   int mDepth;
   int mFeedback;
   double mOutGain;

   wxTextCtrl *mStagesT;
   wxTextCtrl *mDryWetT;
   wxTextCtrl *mFreqT;
   wxTextCtrl *mPhaseT;
   wxTextCtrl *mDepthT;
   wxTextCtrl *mFeedbackT;
   wxTextCtrl *mOutGainT;

   wxSlider *mStagesS;
   wxSlider *mDryWetS;
   wxSlider *mFreqS;
   wxSlider *mPhaseS;
   wxSlider *mDepthS;
   wxSlider *mFeedbackS;
   wxSlider *mOutGainS;

   DECLARE_EVENT_TABLE()
};

#endif
