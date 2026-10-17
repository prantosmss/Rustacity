/**********************************************************************

  Rustacity: A Digital Audio Editor

  Distortion.cpp

  Steve Daulton

  // TODO: Add a graph display of the waveshaper equation.
  // TODO: Allow the user to draw the graph.

******************************************************************//**

\class EffectDistortion
\brief A WaveShaper distortion effect.

*//*******************************************************************/

#include "../Rustacity.h"
#include "Distortion.h"
#include "LoadEffects.h"
#include "rustacity_dsp.h"

// The waveshaper itself lives in the Rust core (rust/dsp, crate
// "rustacity-dsp"); this file is the C++ side of the C ABI declared in
// rust/dsp/include/rustacity_dsp.h.

#include "../Experimental.h"

#include <cmath>
#include <algorithm>
#define _USE_MATH_DEFINES

// Belt and braces
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923132169163975 
#endif

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/intl.h>
#include <wx/valgen.h>
#include <wx/log.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "../Prefs.h"
#include "../Shuttle.h"
#include "../ShuttleGui.h"
#include "../widgets/valnum.h"

// The kinds of table, in the order of the effect's choice list.  The builders
// that use them are the Rust core's (rust/dsp/src/distortion.rs), which has the
// same order; the choice crosses the ABI as the plain index into this list.
enum kTableType
{
   kHardClip,
   kSoftClip,
   kHalfSinCurve,
   kExpCurve,
   kLogCurve,
   kCubic,
   kEvenHarmonics,
   kSinCurve,
   kLeveller,
   kRectifier,
   kHardLimiter,
   nTableTypes
};

static const EnumValueSymbol kTableTypeStrings[nTableTypes] =
{
   { XO("Hard Clipping") },
   { XO("Soft Clipping") },
   { XO("Soft Overdrive") },
   { XO("Medium Overdrive") },
   { XO("Hard Overdrive") },
   { XO("Cubic Curve (odd harmonics)") },
   { XO("Even Harmonics") },
   { XO("Expand and Compress") },
   { XO("Leveller") },
   { XO("Rectifier Distortion") },
   { XO("Hard Limiter 1413") }
};

// Define keys, defaults, minimums, and maximums for the effect parameters
// (Note: 'Repeats' is the total number of times the effect is applied.)
//
//     Name             Type     Key                   Def     Min      Max                 Scale
Param( TableTypeIndx,   int,     wxT("Type"),           0,       0,      nTableTypes-1,    1    );
Param( DCBlock,         bool,    wxT("DC Block"),      false,   false,   true,                1    );
Param( Threshold_dB,    double,  wxT("Threshold dB"),  -6.0,  -100.0,     0.0,             1000.0f );
Param( NoiseFloor,      double,  wxT("Noise Floor"),   -70.0,  -80.0,   -20.0,                1    );
Param( Param1,          double,  wxT("Parameter 1"),    50.0,    0.0,   100.0,                1    );
Param( Param2,          double,  wxT("Parameter 2"),    50.0,    0.0,   100.0,                1    );
Param( Repeats,         int,     wxT("Repeats"),        1,       0,       5,                  1    );

// The number of samples the table was rebuilt after (skipsamples) is gone with
// the code that read it: the Rust core rebuilds the table whenever the settings
// that shape it change, which is the same table.

const double MIN_Threshold_Linear DB_TO_LINEAR(MIN_Threshold_dB);

static const struct
{
   const TranslatableString name;
   EffectDistortion::Params params;
}
FactoryPresets[] =
{
   //                                           Table    DCBlock  threshold   floor       Param1   Param2   Repeats
   // Defaults:                                   0       false   -6.0       -70.0(off)     50.0     50.0     1
   //
   // xgettext:no-c-format
   { XO("Hard clip -12dB, 80% make-up gain"),     { 0,        0,      -12.0,      -70.0,      0.0,     80.0,    0 } },
   // xgettext:no-c-format
   { XO("Soft clip -12dB, 80% make-up gain"),     { 1,        0,      -12.0,      -70.0,      50.0,    80.0,    0 } },
   { XO("Fuzz Box"),                              { 1,        0,      -30.0,      -70.0,      80.0,    80.0,    0 } },
   { XO("Walkie-talkie"),                         { 1,        0,      -50.0,      -70.0,      60.0,    80.0,    0 } },
   { XO("Blues drive sustain"),                   { 2,        0,       -6.0,      -70.0,      30.0,    80.0,    0 } },
   { XO("Light Crunch Overdrive"),                { 3,        0,       -6.0,      -70.0,      20.0,    80.0,    0 } },
   { XO("Heavy Overdrive"),                       { 4,        0,       -6.0,      -70.0,      90.0,    80.0,    0 } },
   { XO("3rd Harmonic (Perfect Fifth)"),          { 5,        0,       -6.0,      -70.0,     100.0,    60.0,    0 } },
   { XO("Valve Overdrive"),                       { 6,        1,       -6.0,      -70.0,      30.0,    40.0,    0 } },
   { XO("2nd Harmonic (Octave)"),                 { 6,        1,       -6.0,      -70.0,      50.0,     0.0,    0 } },
   { XO("Gated Expansion Distortion"),            { 7,        0,       -6.0,      -70.0,      30.0,    80.0,    0 } },
   { XO("Leveller, Light, -70dB noise floor"),    { 8,        0,       -6.0,      -70.0,       0.0,    50.0,    1 } },
   { XO("Leveller, Moderate, -70dB noise floor"), { 8,        0,       -6.0,      -70.0,       0.0,    50.0,    2 } },
   { XO("Leveller, Heavy, -70dB noise floor"),    { 8,        0,       -6.0,      -70.0,       0.0,    50.0,    3 } },
   { XO("Leveller, Heavier, -70dB noise floor"),  { 8,        0,       -6.0,      -70.0,       0.0,    50.0,    4 } },
   { XO("Leveller, Heaviest, -70dB noise floor"), { 8,        0,       -6.0,      -70.0,       0.0,    50.0,    5 } },
   { XO("Half-wave Rectifier"),                   { 9,        0,       -6.0,      -70.0,      50.0,    50.0,    0 } },
   { XO("Full-wave Rectifier"),                   { 9,        0,       -6.0,      -70.0,     100.0,    50.0,    0 } },
   { XO("Full-wave Rectifier (DC blocked)"),      { 9,        1,       -6.0,      -70.0,     100.0,    50.0,    0 } },
   { XO("Percussion Limiter"),                    {10,        0,      -12.0,      -70.0,     100.0,    30.0,    0 } },
};

TranslatableString defaultLabel(int index)
{
   static const TranslatableString names[] = {
      XO("Upper Threshold"),
      XO("Noise Floor"),
      XO("Parameter 1"),
      XO("Parameter 2"),
      XO("Number of repeats"),
   };

   return names[ index ];
}

//
// EffectDistortion
//

const ComponentInterfaceSymbol EffectDistortion::Symbol
{ XO("Distortion") };

namespace{ BuiltinEffectsModule::Registration< EffectDistortion > reg; }

BEGIN_EVENT_TABLE(EffectDistortion, wxEvtHandler)
   EVT_CHOICE(ID_Type, EffectDistortion::OnTypeChoice)
   EVT_CHECKBOX(ID_DCBlock, EffectDistortion::OnDCBlockCheckbox)
   EVT_TEXT(ID_Threshold, EffectDistortion::OnThresholdText)
   EVT_SLIDER(ID_Threshold, EffectDistortion::OnThresholdSlider)
   EVT_TEXT(ID_NoiseFloor, EffectDistortion::OnNoiseFloorText)
   EVT_SLIDER(ID_NoiseFloor, EffectDistortion::OnNoiseFloorSlider)
   EVT_TEXT(ID_Param1, EffectDistortion::OnParam1Text)
   EVT_SLIDER(ID_Param1, EffectDistortion::OnParam1Slider)
   EVT_TEXT(ID_Param2, EffectDistortion::OnParam2Text)
   EVT_SLIDER(ID_Param2, EffectDistortion::OnParam2Slider)
   EVT_TEXT(ID_Repeats, EffectDistortion::OnRepeatsText)
   EVT_SLIDER(ID_Repeats, EffectDistortion::OnRepeatsSlider)
END_EVENT_TABLE()

EffectDistortion::EffectDistortion()
{
   wxASSERT(nTableTypes == WXSIZEOF(kTableTypeStrings));

   mParams.mTableChoiceIndx = DEF_TableTypeIndx;
   mParams.mDCBlock = DEF_DCBlock;
   mParams.mThreshold_dB = DEF_Threshold_dB;
   mThreshold = DB_TO_LINEAR(mParams.mThreshold_dB);
   mParams.mNoiseFloor = DEF_NoiseFloor;
   mParams.mParam1 = DEF_Param1;
   mParams.mParam2 = DEF_Param2;
   mParams.mRepeats = DEF_Repeats;
   mbSavedFilterState = DEF_DCBlock;

   mMaster = nullptr;

   SetLinearEffectFlag(false);
}

EffectDistortion::~EffectDistortion()
{
   rustacity_distortion_free(mMaster);

   for (auto slave : mSlaves)
      rustacity_distortion_free(slave);
}

// ComponentInterface implementation

ComponentInterfaceSymbol EffectDistortion::GetSymbol()
{
   return Symbol;
}

TranslatableString EffectDistortion::GetDescription()
{
   return XO("Waveshaping distortion effect");
}

wxString EffectDistortion::ManualPage()
{
   return wxT("Distortion");
}

// EffectDefinitionInterface implementation

EffectType EffectDistortion::GetType()
{
   return EffectTypeProcess;
}

bool EffectDistortion::SupportsRealtime()
{
#if defined(EXPERIMENTAL_REALTIME_RUSTACITY_EFFECTS)
   return true;
#else
   return false;
#endif
}

// EffectClientInterface implementation

unsigned EffectDistortion::GetAudioInCount()
{
   return 1;
}

unsigned EffectDistortion::GetAudioOutCount()
{
   return 1;
}

bool EffectDistortion::ProcessInitialize(sampleCount WXUNUSED(totalLen), ChannelNames WXUNUSED(chanMap))
{
   // Re-running the effect must not leak the previous run's handle; freeing a
   // null one is a no-op.
   rustacity_distortion_free(mMaster);

   mMaster = rustacity_distortion_new(static_cast<float>(mSampleRate));

   return mMaster != nullptr;
}

size_t EffectDistortion::ProcessBlock(float **inBlock, float **outBlock, size_t blockLen)
{
   return rustacity_distortion_process(mMaster, mParams.mTableChoiceIndx,
                                       mParams.mDCBlock ? 1 : 0,
                                       mParams.mThreshold_dB, mParams.mNoiseFloor,
                                       mParams.mParam1, mParams.mParam2,
                                       mParams.mRepeats, inBlock[0], outBlock[0],
                                       blockLen);
}

bool EffectDistortion::ProcessFinalize()
{
   rustacity_distortion_free(mMaster);
   mMaster = nullptr;

   return true;
}

bool EffectDistortion::RealtimeInitialize()
{
   SetBlockSize(512);

   for (auto slave : mSlaves)
      rustacity_distortion_free(slave);
   mSlaves.clear();

   return true;
}

bool EffectDistortion::RealtimeAddProcessor(unsigned WXUNUSED(numChannels), float sampleRate)
{
   rustacity_distortion* slave = rustacity_distortion_new(sampleRate);

   mSlaves.push_back(slave);

   return slave != nullptr;
}

bool EffectDistortion::RealtimeFinalize()
{
   for (auto slave : mSlaves)
      rustacity_distortion_free(slave);
   mSlaves.clear();

   return true;
}

size_t EffectDistortion::RealtimeProcess(int group,
                                              float **inbuf,
                                              float **outbuf,
                                              size_t numSamples)
{

   return rustacity_distortion_process(mSlaves[group], mParams.mTableChoiceIndx,
                                       mParams.mDCBlock ? 1 : 0,
                                       mParams.mThreshold_dB, mParams.mNoiseFloor,
                                       mParams.mParam1, mParams.mParam2,
                                       mParams.mRepeats, inbuf[0], outbuf[0],
                                       numSamples);
}
bool EffectDistortion::DefineParams( ShuttleParams & S ){
   S.SHUTTLE_ENUM_PARAM( mParams.mTableChoiceIndx, TableTypeIndx,
      kTableTypeStrings, nTableTypes );
   S.SHUTTLE_PARAM( mParams.mDCBlock,       DCBlock       );
   S.SHUTTLE_PARAM( mParams.mThreshold_dB,  Threshold_dB  );
   S.SHUTTLE_PARAM( mParams.mNoiseFloor,    NoiseFloor    );
   S.SHUTTLE_PARAM( mParams.mParam1,        Param1        );
   S.SHUTTLE_PARAM( mParams.mParam2,        Param2        );
   S.SHUTTLE_PARAM( mParams.mRepeats,       Repeats       );
   return true;
}

bool EffectDistortion::GetAutomationParameters(CommandParameters & parms)
{
   parms.Write(KEY_TableTypeIndx,
               kTableTypeStrings[mParams.mTableChoiceIndx].Internal());
   parms.Write(KEY_DCBlock, mParams.mDCBlock);
   parms.Write(KEY_Threshold_dB, mParams.mThreshold_dB);
   parms.Write(KEY_NoiseFloor, mParams.mNoiseFloor);
   parms.Write(KEY_Param1, mParams.mParam1);
   parms.Write(KEY_Param2, mParams.mParam2);
   parms.Write(KEY_Repeats, mParams.mRepeats);

   return true;
}

bool EffectDistortion::SetAutomationParameters(CommandParameters & parms)
{
   ReadAndVerifyEnum(TableTypeIndx, kTableTypeStrings, nTableTypes);
   ReadAndVerifyBool(DCBlock);
   ReadAndVerifyDouble(Threshold_dB);
   ReadAndVerifyDouble(NoiseFloor);
   ReadAndVerifyDouble(Param1);
   ReadAndVerifyDouble(Param2);
   ReadAndVerifyInt(Repeats);

   mParams.mTableChoiceIndx = TableTypeIndx;
   mParams.mDCBlock = DCBlock;
   mParams.mThreshold_dB = Threshold_dB;
   mParams.mNoiseFloor = NoiseFloor;
   mParams.mParam1 = Param1;
   mParams.mParam2 = Param2;
   mParams.mRepeats = Repeats;

   return true;
}

RegistryPaths EffectDistortion::GetFactoryPresets()
{
   RegistryPaths names;

   for (size_t i = 0; i < WXSIZEOF(FactoryPresets); i++)
   {
      names.push_back( FactoryPresets[i].name.Translation() );
   }

   return names;
}

bool EffectDistortion::LoadFactoryPreset(int id)
{
   if (id < 0 || id >= (int) WXSIZEOF(FactoryPresets))
   {
      return false;
   }

   mParams = FactoryPresets[id].params;
   mThreshold = DB_TO_LINEAR(mParams.mThreshold_dB);

   if (mUIDialog)
   {
      TransferDataToWindow();
   }

   return true;
}


// Effect implementation

void EffectDistortion::PopulateOrExchange(ShuttleGui & S)
{
   S.AddSpace(0, 5);
   S.StartVerticalLay();
   {
      S.StartMultiColumn(4, wxCENTER);
      {
         mTypeChoiceCtrl = S.Id(ID_Type)
            .MinSize( { -1, -1 } )
            .Validator<wxGenericValidator>(&mParams.mTableChoiceIndx)
            .AddChoice(XO("Distortion type:"),
               Msgids(kTableTypeStrings, nTableTypes));

         mDCBlockCheckBox = S.Id(ID_DCBlock).AddCheckBox(XO("DC blocking filter"),
                                       DEF_DCBlock);
      }
      S.EndMultiColumn();
      S.AddSpace(0, 10);


      S.StartStatic(XO("Threshold controls"));
      {
         S.StartMultiColumn(4, wxEXPAND);
         S.SetStretchyCol(2);
         {
            // Allow space for first Column
            S.AddSpace(250,0); S.AddSpace(0,0); S.AddSpace(0,0); S.AddSpace(0,0); 

            // Upper threshold control
            mThresholdTxt = S.AddVariableText(defaultLabel(0),
               false, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
            mThresholdT = S.Id(ID_Threshold)
               .Name(defaultLabel(0))
               .Validator<FloatingPointValidator<double>>(
                  2, &mParams.mThreshold_dB, NumValidatorStyle::DEFAULT,
                  MIN_Threshold_dB, MAX_Threshold_dB)
               .AddTextBox( {}, wxT(""), 10);

            mThresholdS = S.Id(ID_Threshold)
               .Name(defaultLabel(0))
               .Style(wxSL_HORIZONTAL)
               .AddSlider( {}, 0,
                  DB_TO_LINEAR(MAX_Threshold_dB) * SCL_Threshold_dB,
                  DB_TO_LINEAR(MIN_Threshold_dB) * SCL_Threshold_dB);
            S.AddSpace(20, 0);

            // Noise floor control
            mNoiseFloorTxt = S.AddVariableText(defaultLabel(1),
               false, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
            mNoiseFloorT = S.Id(ID_NoiseFloor)
               .Name(defaultLabel(1))
               .Validator<FloatingPointValidator<double>>(
                  2, &mParams.mNoiseFloor, NumValidatorStyle::DEFAULT,
                  MIN_NoiseFloor, MAX_NoiseFloor
               )
               .AddTextBox( {}, wxT(""), 10);

            mNoiseFloorS = S.Id(ID_NoiseFloor)
               .Name(defaultLabel(1))
               .Style(wxSL_HORIZONTAL)
               .AddSlider( {}, 0, MAX_NoiseFloor, MIN_NoiseFloor);
            S.AddSpace(20, 0);
         }
         S.EndMultiColumn();
      }
      S.EndStatic();

      S.StartStatic(XO("Parameter controls"));
      {
         S.StartMultiColumn(4, wxEXPAND);
         S.SetStretchyCol(2);
         {
            // Allow space for first Column
            S.AddSpace(250,0); S.AddSpace(0,0); S.AddSpace(0,0); S.AddSpace(0,0); 

            // Parameter1 control
            mParam1Txt = S.AddVariableText(defaultLabel(2),
               false, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
            mParam1T = S.Id(ID_Param1)
               .Name(defaultLabel(2))
               .Validator<FloatingPointValidator<double>>(
                  2, &mParams.mParam1, NumValidatorStyle::DEFAULT,
                  MIN_Param1, MAX_Param1
               )
               .AddTextBox( {}, wxT(""), 10);

            mParam1S = S.Id(ID_Param1)
               .Name(defaultLabel(2))
               .Style(wxSL_HORIZONTAL)
               .AddSlider( {}, 0, MAX_Param1, MIN_Param1);
            S.AddSpace(20, 0);

            // Parameter2 control
            mParam2Txt = S.AddVariableText(defaultLabel(3),
               false, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
            mParam2T = S.Id(ID_Param2)
               .Name(defaultLabel(3))
               .Validator<FloatingPointValidator<double>>(
                  2, &mParams.mParam2, NumValidatorStyle::DEFAULT,
                  MIN_Param2, MAX_Param2
               )
               .AddTextBox( {}, wxT(""), 10);

            mParam2S = S.Id(ID_Param2)
               .Name(defaultLabel(3))
               .Style(wxSL_HORIZONTAL)
               .AddSlider( {}, 0, MAX_Param2, MIN_Param2);
            S.AddSpace(20, 0);

            // Repeats control
            mRepeatsTxt = S.AddVariableText(defaultLabel(4),
               false, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
            mRepeatsT = S.Id(ID_Repeats)
               .Name(defaultLabel(4))
               .Validator<IntegerValidator<int>>(
                  &mParams.mRepeats, NumValidatorStyle::DEFAULT,
                  MIN_Repeats, MAX_Repeats
               )
               .AddTextBox( {}, wxT(""), 10);

            mRepeatsS = S.Id(ID_Repeats)
               .Name(defaultLabel(4))
               .Style(wxSL_HORIZONTAL)
               .AddSlider( {}, DEF_Repeats, MAX_Repeats, MIN_Repeats);
            S.AddSpace(20, 0);
         }
         S.EndMultiColumn();
      }
      S.EndStatic();
   }
   S.EndVerticalLay();

   return;
}

bool EffectDistortion::TransferDataToWindow()
{
   if (!mUIParent->TransferDataToWindow())
   {
      return false;
   }

   mThresholdS->SetValue((int) (mThreshold * SCL_Threshold_dB + 0.5));
   mDCBlockCheckBox->SetValue(mParams.mDCBlock);
   mNoiseFloorS->SetValue((int) mParams.mNoiseFloor + 0.5);
   mParam1S->SetValue((int) mParams.mParam1 + 0.5);
   mParam2S->SetValue((int) mParams.mParam2 + 0.5);
   mRepeatsS->SetValue(mParams.mRepeats);

   mbSavedFilterState = mParams.mDCBlock;

   UpdateUI();

   return true;
}

bool EffectDistortion::TransferDataFromWindow()
{
   if (!mUIParent->Validate() || !mUIParent->TransferDataFromWindow())
   {
      return false;
   }

   mThreshold = DB_TO_LINEAR(mParams.mThreshold_dB);

   return true;
}

void EffectDistortion::OnTypeChoice(wxCommandEvent& /*evt*/)
{
   mTypeChoiceCtrl->GetValidator()->TransferFromWindow();

   UpdateUI();
}

void EffectDistortion::OnDCBlockCheckbox(wxCommandEvent& /*evt*/)
{
   mParams.mDCBlock = mDCBlockCheckBox->GetValue();
   mbSavedFilterState = mParams.mDCBlock;
}


void EffectDistortion::OnThresholdText(wxCommandEvent& /*evt*/)
{
   mThresholdT->GetValidator()->TransferFromWindow();
   mThreshold = DB_TO_LINEAR(mParams.mThreshold_dB);
   mThresholdS->SetValue((int) (mThreshold * SCL_Threshold_dB + 0.5));
}

void EffectDistortion::OnThresholdSlider(wxCommandEvent& evt)
{
   mThreshold = (double) evt.GetInt() / SCL_Threshold_dB;
   mParams.mThreshold_dB = wxMax(LINEAR_TO_DB(mThreshold), MIN_Threshold_dB);
   mThreshold = std::max(MIN_Threshold_Linear, mThreshold);
   mThresholdT->GetValidator()->TransferToWindow();
}

void EffectDistortion::OnNoiseFloorText(wxCommandEvent& /*evt*/)
{
   mNoiseFloorT->GetValidator()->TransferFromWindow();
   mNoiseFloorS->SetValue((int) floor(mParams.mNoiseFloor + 0.5));
}

void EffectDistortion::OnNoiseFloorSlider(wxCommandEvent& evt)
{
   mParams.mNoiseFloor = (double) evt.GetInt();
   mNoiseFloorT->GetValidator()->TransferToWindow();
}


void EffectDistortion::OnParam1Text(wxCommandEvent& /*evt*/)
{
   mParam1T->GetValidator()->TransferFromWindow();
   mParam1S->SetValue((int) floor(mParams.mParam1 + 0.5));
}

void EffectDistortion::OnParam1Slider(wxCommandEvent& evt)
{
   mParams.mParam1 = (double) evt.GetInt();
   mParam1T->GetValidator()->TransferToWindow();
}

void EffectDistortion::OnParam2Text(wxCommandEvent& /*evt*/)
{
   mParam2T->GetValidator()->TransferFromWindow();
   mParam2S->SetValue((int) floor(mParams.mParam2 + 0.5));
}

void EffectDistortion::OnParam2Slider(wxCommandEvent& evt)
{
   mParams.mParam2 = (double) evt.GetInt();
   mParam2T->GetValidator()->TransferToWindow();
}

void EffectDistortion::OnRepeatsText(wxCommandEvent& /*evt*/)
{
   mRepeatsT->GetValidator()->TransferFromWindow();
   mRepeatsS->SetValue(mParams.mRepeats);
}

void EffectDistortion::OnRepeatsSlider(wxCommandEvent& evt)
{
   mParams.mRepeats = evt.GetInt();
   mRepeatsT->GetValidator()->TransferToWindow();
   
}

void EffectDistortion::UpdateUI()
{
   // set control text and names to match distortion type
   switch (mParams.mTableChoiceIndx)
      {
      case kHardClip:
         UpdateControlText(mThresholdT, mOldThresholdTxt, true);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, true, XO("Clipping level"));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Drive"));
         UpdateControl(ID_Param2, true, XO("Make-up Gain"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kSoftClip:
         UpdateControlText(mThresholdT, mOldThresholdTxt, true);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, true, XO("Clipping threshold"));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Hardness"));
         UpdateControl(ID_Param2, true, XO("Make-up Gain"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kHalfSinCurve:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, true, XO("Output level"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kExpCurve:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, true, XO("Output level"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kLogCurve:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, true, XO("Output level"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kCubic:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, true);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, true, XO("Output level"));
         UpdateControl(ID_Repeats, true, XO("Repeat processing"));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kEvenHarmonics:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, true, XO("Harmonic brightness"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, true, {});
         break;
      case kSinCurve:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, true, XO("Output level"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kLeveller:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, true);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, false);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, true);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, true, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Levelling fine adjustment"));
         UpdateControl(ID_Param2, false, defaultLabel(3));
         UpdateControl(ID_Repeats, true, XO("Degree of Levelling"));
         UpdateControl(ID_DCBlock, false, {});
         break;
      case kRectifier:
         UpdateControlText(mThresholdT, mOldThresholdTxt, false);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, false);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, false, defaultLabel(0));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Distortion amount"));
         UpdateControl(ID_Param2, false, defaultLabel(3));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, true, {});
         break;
      case kHardLimiter:
         UpdateControlText(mThresholdT, mOldThresholdTxt, true);
         UpdateControlText(mNoiseFloorT, mOldmNoiseFloorTxt, false);
         UpdateControlText(mParam1T, mOldParam1Txt, true);
         UpdateControlText(mParam2T, mOldParam2Txt, true);
         UpdateControlText(mRepeatsT, mOldRepeatsTxt, false);

         UpdateControl(ID_Threshold, true, XO("dB Limit"));
         UpdateControl(ID_NoiseFloor, false, defaultLabel(1));
         UpdateControl(ID_Param1, true, XO("Wet level"));
         UpdateControl(ID_Param2, true, XO("Residual level"));
         UpdateControl(ID_Repeats, false, defaultLabel(4));
         UpdateControl(ID_DCBlock, false, {});
         break;
      default:
         UpdateControl(ID_Threshold,   true, defaultLabel(0));
         UpdateControl(ID_NoiseFloor,  true, defaultLabel(1));
         UpdateControl(ID_Param1,      true, defaultLabel(2));
         UpdateControl(ID_Param2,      true, defaultLabel(3));
         UpdateControl(ID_Repeats,     true, defaultLabel(4));
         UpdateControl(ID_DCBlock,     false, {});
   }
}

void EffectDistortion::UpdateControl(
   control id, bool enabled, TranslatableString name)
{
   auto suffix = XO("(Not Used):");
   switch (id)
   {
      case ID_Threshold: {
         /* i18n-hint: Control range. */
         if (enabled) suffix = XO("(-100 to 0 dB):");
         name.Join( suffix, wxT(" ") );

         // Logarithmic slider is set indirectly
         mThreshold = DB_TO_LINEAR(mParams.mThreshold_dB);
         mThresholdS->SetValue((int) (mThreshold * SCL_Threshold_dB + 0.5));

         auto translated = name.Translation();
         mThresholdTxt->SetLabel(translated);
         mThresholdS->SetName(translated);
         mThresholdT->SetName(translated);
         mThresholdS->Enable(enabled);
         mThresholdT->Enable(enabled);
         break;
      }
      case ID_NoiseFloor: {
         /* i18n-hint: Control range. */
         if (enabled) suffix = XO("(-80 to -20 dB):");
         name.Join( suffix, wxT(" ") );

         auto translated = name.Translation();
         mNoiseFloorTxt->SetLabel(translated);
         mNoiseFloorS->SetName(translated);
         mNoiseFloorT->SetName(translated);
         mNoiseFloorS->Enable(enabled);
         mNoiseFloorT->Enable(enabled);
         break;
      }
      case ID_Param1: {
         /* i18n-hint: Control range. */
         if (enabled) suffix = XO("(0 to 100):");
         name.Join( suffix, wxT(" ") );

         auto translated = name.Translation();
         mParam1Txt->SetLabel(translated);
         mParam1S->SetName(translated);
         mParam1T->SetName(translated);
         mParam1S->Enable(enabled);
         mParam1T->Enable(enabled);
         break;
      }
      case ID_Param2: {
         /* i18n-hint: Control range. */
         if (enabled) suffix = XO("(0 to 100):");
         name.Join( suffix, wxT(" ") );

         auto translated = name.Translation();
         mParam2Txt->SetLabel(translated);
         mParam2S->SetName(translated);
         mParam2T->SetName(translated);
         mParam2S->Enable(enabled);
         mParam2T->Enable(enabled);
         break;
      }
      case ID_Repeats: {
         /* i18n-hint: Control range. */
         if (enabled) suffix = XO("(0 to 5):");
         name.Join( suffix, wxT(" ") );

         auto translated = name.Translation();
         mRepeatsTxt->SetLabel(translated);
         mRepeatsS->SetName(translated);
         mRepeatsT->SetName(translated);
         mRepeatsS->Enable(enabled);
         mRepeatsT->Enable(enabled);
         break;
      }
      case ID_DCBlock: {
         if (enabled) {
            mDCBlockCheckBox->SetValue(mbSavedFilterState);
            mParams.mDCBlock = mbSavedFilterState;
         }
         else {
            mDCBlockCheckBox->SetValue(false);
            mParams.mDCBlock = false;
         }

         mDCBlockCheckBox->Enable(enabled);
         break;
      }
      default:
         break;
   }
}

void EffectDistortion::UpdateControlText(wxTextCtrl* textCtrl, wxString& string, bool enabled)
{
   if (enabled) {
      if (textCtrl->GetValue().empty())
         textCtrl->SetValue(string);
      else
         string = textCtrl->GetValue();
   }
   else {
      if (!textCtrl->GetValue().empty())
         string = textCtrl->GetValue();
      textCtrl->SetValue(wxT(""));
   }
}

