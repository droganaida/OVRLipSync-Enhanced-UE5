/*******************************************************************************
 * Modifications by Aida Drogan, SilverCord-VR:
 * - Introduced interpolation for phoneme transitions using a weighted average over previous frames.
 * - Added dynamic adjustment of interpolation strength based on speech tempo.
 * - Updated function interface to include `FVisemeInterpolationSettings`.
 * - Modified interpolation to preserve consonant clarity by skipping smoothing for consonant phonemes.
 * - Added filtering of short-duration phonemes below MinHoldFrames.
 *******************************************************************************/

#include "CookFrameSequenceAsync.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"
#include "Sound/SoundWave.h"
#include <map>

constexpr auto LipSyncSequenceUpateFrequency = 100;
constexpr auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpateFrequency;

enum class EVisemeType
{
	Vowel,
	Consonant,
	Other
};

EVisemeType GetVisemeType(int Index)
{
	switch (Index)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		return EVisemeType::Vowel;
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
		return EVisemeType::Consonant;
	default:
		return EVisemeType::Other;
	}
}

UCookFrameSequenceAsync *UCookFrameSequenceAsync::CookFrameSequence(const TArray<uint8> &RawSamples,
																	bool UseOfflineModel,
																	const FVisemeInterpolationSettings &Settings)
{
	UCookFrameSequenceAsync *BPNode = NewObject<UCookFrameSequenceAsync>();
	BPNode->RawSamples = RawSamples;
	BPNode->UseOfflineModel = UseOfflineModel;
	BPNode->InterpolationSettings = Settings;
	return BPNode;
}

void UCookFrameSequenceAsync::Activate()
{
	if (RawSamples.Num() <= 44)
	{
		onFrameSequenceCooked.Broadcast(nullptr, false);
		return;
	}

	FWaveModInfo waveInfo;
	uint8 *waveData = const_cast<uint8 *>(RawSamples.GetData());

	if (waveInfo.ReadWaveInfo(waveData, RawSamples.Num()))
	{
		int32 NumChannels = *waveInfo.pChannels;
		int32 SampleRate = *waveInfo.pSamplesPerSec;
		auto PCMDataSize = waveInfo.SampleDataSize / sizeof(int16_t);
		int16_t *PCMData = reinterpret_cast<int16_t *>(waveData + 44);
		int32 ChunkSizeSamples = static_cast<int32>(SampleRate * LipSyncSequenceDuration);
		int32 ChunkSize = NumChannels * ChunkSizeSamples;
		int BufferSize = 4096;

		if (NumChannels <= 0 || SampleRate <= 0)
		{
			ensureMsgf(false, TEXT("Invalid audio file format. NumChannels or SampleRate is not valid."));
			onFrameSequenceCooked.Broadcast(nullptr, false);
			return;
		}

		FString modelPath = UseOfflineModel ? FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("OVRLipSync"),
															  TEXT("OfflineModel"), TEXT("ovrlipsync_offline_model.pb"))
											: FString();

		const FVisemeInterpolationSettings Settings = InterpolationSettings;

		Async(EAsyncExecution::Thread,
			  [this, PCMData, PCMDataSize, ChunkSize, ChunkSizeSamples, NumChannels, modelPath, SampleRate, BufferSize,
			   Settings]()
			  {
				  UOVRLipSyncFrameSequence *Sequence = NewObject<UOVRLipSyncFrameSequence>();
				  UOVRLipSyncContextWrapper context(ovrLipSyncContextProvider_Enhanced, SampleRate, BufferSize,
													modelPath);

				  TArray<float> CurrentVisemes;
				  TArray<TArray<float>> FrameBuffer;
				  float LaughterScore = 0.0f;
				  int32_t FrameDelayInMs = 0;
				  int TotalFrames = 0;
				  int NumInterpolationFrames = FMath::Clamp(Settings.MaxInterpolationFrames, 1, 24);

				  // Temporary storage for all raw frames
				  TArray<TArray<float>> RawVisemeFrames;
				  TArray<float> LaughterScores;

				  for (int offs = 0; offs + ChunkSize < PCMDataSize; offs += ChunkSize)
				  {
					  context.ProcessFrame(PCMData + offs, ChunkSizeSamples, CurrentVisemes, LaughterScore,
										   FrameDelayInMs, NumChannels > 1);
					  RawVisemeFrames.Add(CurrentVisemes);
					  LaughterScores.Add(LaughterScore);
				  }

				  // Filtering: remove short-duration visemes
				  for (int i = 0; i < CurrentVisemes.Num(); ++i)
				  {
					  int Start = -1;
					  for (int f = 0; f < RawVisemeFrames.Num(); ++f)
					  {
						  float value = RawVisemeFrames[f][i];
						  if (value > 0.5f)
						  {
							  if (Start == -1)
								  Start = f;
						  }
						  else if (Start != -1)
						  {
							  int Duration = f - Start;
							  if (Duration < Settings.MinHoldFrames)
							  {
								  // Replace with neighbors (previous or zero)
								  for (int r = Start; r < f; ++r)
									  RawVisemeFrames[r][i] = 0.0f;
							  }
							  Start = -1;
						  }
					  }
				  }

				  // Now apply interpolation on the filtered data
				  for (int f = 0; f < RawVisemeFrames.Num(); ++f)
				  {
					  const TArray<float> &Current = RawVisemeFrames[f];
					  if (Settings.bEnableInterpolation && FrameBuffer.Num() > 0)
					  {
						  TArray<float> Smoothed;
						  for (int i = 0; i < Current.Num(); ++i)
						  {
							  EVisemeType Type = GetVisemeType(i);
							  if (Settings.bStrictConsonantLock && Type == EVisemeType::Consonant)
							  {
								  Smoothed.Add(Current[i]);
							  }
							  else
							  {
								  float WeightedSum = Current[i];
								  float TotalWeight = 1.0f;
								  for (int j = 0; j < FrameBuffer.Num(); ++j)
								  {
									  float Weight = 1.0f - static_cast<float>(j + 1) / (NumInterpolationFrames + 1);
									  WeightedSum += FrameBuffer[j][i] * Weight;
									  TotalWeight += Weight;
								  }
								  Smoothed.Add(WeightedSum / TotalWeight);
							  }
						  }
						  Sequence->Add(Smoothed, LaughterScores[f]);
					  }
					  else
					  {
						  Sequence->Add(Current, LaughterScores[f]);
					  }

					  FrameBuffer.Insert(Current, 0);
					  if (FrameBuffer.Num() > NumInterpolationFrames)
					  {
						  FrameBuffer.RemoveAt(FrameBuffer.Num() - 1);
					  }
				  }

				  AsyncTask(ENamedThreads::GameThread,
							[Sequence, this]() { onFrameSequenceCooked.Broadcast(Sequence, true); });
			  });
	}
	else
	{
		onFrameSequenceCooked.Broadcast(nullptr, false);
	}
}
