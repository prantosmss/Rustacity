/**********************************************************************

   Rustacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2016 Audacity Team.
   License: GPL v2.  See License.txt.

   BassTreble.h (two shelf filters)
   Steve Daulton

**********************************************************************/

#ifndef __RUSTACITY_EFFECT_BASS_TREBLE__
#define __RUSTACITY_EFFECT_BASS_TREBLE__

#include "Effect.h"

/// The shelf filters live in the Rust DSP core (rust/dsp); this is an
/// opaque handle to them.  See rust/dsp/include/rustacity_dsp.h for the
/// C ABI.
struct rustacity_bass_treble;

class wxSlider;
class wxCheckBox;
class wxTextCtrl;
class ShuttleGui;

class EffectBassTreble final : public Effect
{
public:
   static const ComponentInterfaceSymbol Symbol;

   EffectBassTreble();
   virtual ~EffectBassTreble();

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


   // Effect Implementation

   void PopulateOrExchange(ShuttleGui & S) override;
   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;

   bool CheckWhetherSkipEffect() override;

private:
   void OnBassText(wxCommandEvent & evt);
   void OnTrebleText(wxCommandEvent & evt);
   void OnGainText(wxCommandEvent & evt);
   void OnBassSlider(wxCommandEvent & evt);
   void OnTrebleSlider(wxCommandEvent & evt);
   void OnGainSlider(wxCommandEvent & evt);
   void OnLinkCheckbox(wxCommandEvent & evt);

   // Auto-adjust gain to reduce variation in peak level
   void UpdateGain(double oldVal, int control );

private:
   rustacity_bass_treble* mMaster;
   std::vector<rustacity_bass_treble*> mSlaves;

   double      mBass;
   double      mTreble;
   double      mGain;
   bool        mLink;

   wxSlider    *mBassS;
   wxSlider    *mTrebleS;
   wxSlider    *mGainS;

   wxTextCtrl  *mBassT;
   wxTextCtrl  *mTrebleT;
   wxTextCtrl  *mGainT;

   wxCheckBox  *mLinkCheckBox;

   DECLARE_EVENT_TABLE()
};

#endif
