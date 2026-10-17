/**********************************************************************

  Rustacity: A Digital Audio Editor

  Distortion.h

  Steve Daulton

**********************************************************************/

#ifndef __RUSTACITY_EFFECT_DISTORTION__
#define __RUSTACITY_EFFECT_DISTORTION__

#include "DistortionFilter.h"
#include "Effect.h"

class wxSlider;
class wxStaticText;
class wxCheckBox;
class wxTextCtrl;
class ShuttleGui;

class EffectDistortion final : public Effect
{
public:
   static const ComponentInterfaceSymbol Symbol;

   EffectDistortion();
   virtual ~EffectDistortion();

   struct Params
   {
      int    mTableChoiceIndx;
      bool   mDCBlock;
      double mThreshold_dB;
      double mNoiseFloor;
      double mParam1;
      double mParam2;
      int    mRepeats;
   };

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
   RegistryPaths GetFactoryPresets() override;
   bool LoadFactoryPreset(int id) override;

   // Effect implementation

   void PopulateOrExchange(ShuttleGui & S) override;
   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;

private:

   enum control
   {
      ID_Type = 10000,
      ID_DCBlock,
      ID_Threshold,
      ID_NoiseFloor,
      ID_Param1,
      ID_Param2,
      ID_Repeats,
   };
   // EffectDistortion implementation

   void InstanceInit(EffectDistortionState & data, float sampleRate);
   size_t InstanceProcess(EffectDistortionState & data,
                               float **inBlock,
                               float **outBlock,
                               size_t blockLen);

   // Control Handlers

   void OnTypeChoice(wxCommandEvent & evt);
   void OnDCBlockCheckbox(wxCommandEvent & evt);
   void OnThresholdText(wxCommandEvent & evt);
   void OnThresholdSlider(wxCommandEvent & evt);
   void OnNoiseFloorText(wxCommandEvent & evt);
   void OnNoiseFloorSlider(wxCommandEvent & evt);
   void OnParam1Text(wxCommandEvent & evt);
   void OnParam1Slider(wxCommandEvent & evt);
   void OnParam2Text(wxCommandEvent & evt);
   void OnParam2Slider(wxCommandEvent & evt);
   void OnRepeatsText(wxCommandEvent & evt);
   void OnRepeatsSlider(wxCommandEvent & evt);
   void UpdateUI();
   void UpdateControl(control id, bool enable, TranslatableString name);
   void UpdateControlText(wxTextCtrl *textCtrl, wxString &string, bool enabled);

private:
   EffectDistortionState mMaster;
   std::vector<EffectDistortionState> mSlaves;

   double mThreshold;
   bool mbSavedFilterState;

   int mTypChoiceIndex;

   wxChoice *mTypeChoiceCtrl;
   wxTextCtrl *mThresholdT;
   wxTextCtrl *mNoiseFloorT;
   wxTextCtrl *mParam1T;
   wxTextCtrl *mParam2T;
   wxTextCtrl *mRepeatsT;

   wxSlider *mThresholdS;
   wxSlider *mNoiseFloorS;
   wxSlider *mParam1S;
   wxSlider *mParam2S;
   wxSlider *mRepeatsS;

   wxCheckBox *mDCBlockCheckBox;

   wxStaticText *mThresholdTxt;
   wxStaticText *mNoiseFloorTxt;
   wxStaticText *mParam1Txt;
   wxStaticText *mParam2Txt;
   wxStaticText *mRepeatsTxt;
   
   wxString mOldThresholdTxt;
   wxString mOldmNoiseFloorTxt;
   wxString mOldParam1Txt;
   wxString mOldParam2Txt;
   wxString mOldRepeatsTxt;

   Params mParams;

   DECLARE_EVENT_TABLE()
};

#endif
