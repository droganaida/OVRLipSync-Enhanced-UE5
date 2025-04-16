/*******************************************************************************
 * Modifications by Aida Drogan, SilverCord-VR:
 * - Introduced interpolation for phoneme transitions using a weighted average over previous frames.
 * - Added dynamic adjustment of interpolation strength based on speech tempo.
 * - Updated function interface to include `FVisemeInterpolationSettings`.
 * - Modified interpolation to preserve consonant clarity by applying scaling during smoothing.
 * - Added filtering of short-duration phonemes below MinHoldFrames.
 * - Added block-based viseme clustering using NumInterpolationFrames.
 * - Added weighted priority for dominant viseme selection.
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
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
		return EVisemeType::Vowel;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
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

	if (!waveInfo.ReadWaveInfo(waveData, RawSamples.Num()))
	{
		onFrameSequenceCooked.Broadcast(nullptr, false);
		return;
	}

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
			  UOVRLipSyncContextWrapper context(ovrLipSyncContextProvider_Enhanced, SampleRate, BufferSize, modelPath);

			  TArray<TArray<float>> RawVisemeFrames;
			  TArray<float> LaughterScores;
			  TArray<TArray<float>> FrameBuffer;
			  TArray<float> CurrentVisemes;
			  float LaughterScore = 0.0f;
			  int32 FrameDelayInMs = 0;
			  int NumInterpolationFrames = FMath::Clamp(Settings.MaxInterpolationFrames, 1, 24);

			  // Generate raw frame data
			  for (int offs = 0; offs + ChunkSize < PCMDataSize; offs += ChunkSize)
			  {
				  context.ProcessFrame(PCMData + offs, ChunkSizeSamples, CurrentVisemes, LaughterScore, FrameDelayInMs,
									   NumChannels > 1);
				  RawVisemeFrames.Add(CurrentVisemes);
				  LaughterScores.Add(LaughterScore);
			  }

			  // Step 1: filter out short visemes
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
							  for (int r = Start; r < f; ++r)
								  RawVisemeFrames[r][i] = 0.0f;
						  }
						  Start = -1;
					  }
				  }
			  }

			  // Step 2: cluster into blocks and scale dominant viseme (with priority)
			  TArray<int> BlockDominants;
			  TArray<float> BlockPeaks;

			  TMap<int, float> VisemePriority = {
				  {10, 1.0f}, // aa
				  {11, 0.6f}, // E
				  {12, 0.5f}, // ih
				  {13, 0.9f}, // oh
				  {14, 1.0f}, // ou
				  {1, 0.9f},  // PP
				  {2, 0.7f},  // FF
				  {3, 0.6f},  // TH
				  {4, 0.7f},  // DD
				  {5, 0.7f},  // kk
				  {6, 0.8f},  // CH
				  {7, 0.6f},  // SS
				  {8, 0.7f},  // nn
				  {9, 0.9f}	  // RR
			  };

			  for (int blockStart = 0; blockStart < RawVisemeFrames.Num(); blockStart += NumInterpolationFrames)
			  {
				  int blockEnd = FMath::Min(blockStart + NumInterpolationFrames, RawVisemeFrames.Num());

				  int DominantIndex = -1;
				  float MaxSum = 0.0f;

				  for (int j = 0; j < RawVisemeFrames[0].Num(); ++j)
				  {
					  float Priority = VisemePriority.Contains(j) ? VisemePriority[j] : 1.0f;
					  float Sum = 0.0f;
					  for (int f = blockStart; f < blockEnd; ++f)
						  Sum += RawVisemeFrames[f][j];

					  Sum *= Priority;

					  if (Sum > MaxSum)
					  {
						  MaxSum = Sum;
						  DominantIndex = j;
					  }
				  }

				  if (DominantIndex < 0)
				  {
					  BlockDominants.Add(-1);
					  BlockPeaks.Add(0.0f);
					  continue;
				  }

				  BlockDominants.Add(DominantIndex);

				  float PeakValue = 0.0f;
				  for (int f = blockStart; f < blockEnd; ++f)
				  {
					  float value = RawVisemeFrames[f][DominantIndex];
					  if (value > PeakValue)
						  PeakValue = value;
				  }
				  BlockPeaks.Add(PeakValue);
			  }

			  // Step 3: apply scaled dominant viseme in block, preserve neighbors
			  for (int blockIndex = 0; blockIndex < BlockDominants.Num(); ++blockIndex)
			  {
				  int blockStart = blockIndex * NumInterpolationFrames;
				  int blockEnd = FMath::Min(blockStart + NumInterpolationFrames, RawVisemeFrames.Num());

				  int DominantIndex = BlockDominants[blockIndex];
				  float Peak = BlockPeaks[blockIndex];
				  if (Peak <= 0.0001f)
					  continue;

				  float Scale = 1.0f / Peak;

				  int PrevDominant = (blockIndex > 0) ? BlockDominants[blockIndex - 1] : -1;
				  int NextDominant = (blockIndex < BlockDominants.Num() - 1) ? BlockDominants[blockIndex + 1] : -1;

				  bool IsFirstBlock = (blockIndex == 0);
				  bool IsLastBlock = (blockIndex == BlockDominants.Num() - 1);

				  for (int f = blockStart; f < blockEnd; ++f)
				  {
					  int localIndex = f - blockStart;
					  for (int j = 0; j < RawVisemeFrames[f].Num(); ++j)
					  {
						  bool bPreserve =
							  (j == DominantIndex) ||
							  (j == PrevDominant && localIndex < NumInterpolationFrames / 2 && !IsFirstBlock) ||
							  (j == NextDominant && localIndex >= NumInterpolationFrames / 2 && !IsLastBlock);

						  if (bPreserve)
						  {
							  if (j == DominantIndex)
								  RawVisemeFrames[f][j] = FMath::Clamp(RawVisemeFrames[f][j] * Scale, 0.0f, 1.0f);
						  }
						  else
						  {
							  RawVisemeFrames[f][j] = 0.0f;
						  }
					  }
				  }
			  }

			  // Step 4: final smoothing
			  for (int f = 0; f < RawVisemeFrames.Num(); ++f)
			  {
				  const TArray<float> &Current = RawVisemeFrames[f];

				  if (Settings.bEnableInterpolation && FrameBuffer.Num() > 0)
				  {
					  TArray<float> Smoothed;
					  for (int i = 0; i < Current.Num(); ++i)
					  {
						  float WeightedSum = Current[i];
						  float TotalWeight = 1.0f;

						  for (int j = 0; j < FrameBuffer.Num(); ++j)
						  {
							  float Weight = 1.0f - static_cast<float>(j + 1) / (NumInterpolationFrames + 1);
							  WeightedSum += FrameBuffer[j][i] * Weight;
							  TotalWeight += Weight;
						  }

						  float Value = WeightedSum / TotalWeight;

						  if (Settings.bStrictConsonantLock && GetVisemeType(i) == EVisemeType::Consonant &&
							  Current[i] > 0.5f)
						  {
							  float Scale = 1.0f / Current[i];
							  Value = FMath::Clamp(Value * Scale, 0.0f, 1.0f);
						  }

						  Smoothed.Add(Value);
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
