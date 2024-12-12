/*******************************************************************************
 * Modifications by Aida Drogan, SilverCord-VR:
 * - Introduced interpolation for phoneme transitions using a weighted average over previous frames.
 * - Added dynamic adjustment of interpolation strength based on speech tempo.
 * - Updated function interface to include `bEnableInterpolation` and `MaxInterpolationFrames`.
 *******************************************************************************/

#include "CookFrameSequenceAsync.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"
#include "Sound/SoundWave.h"
#include <map>

constexpr auto LipSyncSequenceUpateFrequency = 100;
constexpr auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpateFrequency;

UCookFrameSequenceAsync *UCookFrameSequenceAsync::CookFrameSequence(const TArray<uint8> &RawSamples,
																	bool UseOfflineModel, bool bEnableInterpolation,
																	int MaxInterpolationFrames)
{
	// Initialize the asynchronous operation for generating lip-sync sequences.
	// Added new parameters:
	// - bEnableInterpolation: Enables or disables phoneme interpolation.
	// - MaxInterpolationFrames: Specifies the maximum number of frames to use for interpolation.
	
	UCookFrameSequenceAsync *BPNode = NewObject<UCookFrameSequenceAsync>();

	BPNode->RawSamples = RawSamples;
	BPNode->UseOfflineModel = UseOfflineModel;
	BPNode->bEnableInterpolation = bEnableInterpolation;
	BPNode->MaxInterpolationFrames = FMath::Clamp(MaxInterpolationFrames, 1, 24);

	return BPNode;
}

void UCookFrameSequenceAsync::Activate()
{
	// Validate raw audio data and prepare for processing
	if (RawSamples.Num() <= 44)
	{
		onFrameSequenceCooked.Broadcast(nullptr, false);
		return;
	}

	FWaveModInfo waveInfo;
	uint8 *waveData = const_cast<uint8 *>(RawSamples.GetData());

	// Validate WAV header information and extract audio parameters
	if (waveInfo.ReadWaveInfo(waveData, RawSamples.Num()))
	{
		int32 NumChannels = *waveInfo.pChannels;
		int32 SampleRate = *waveInfo.pSamplesPerSec;
		auto PCMDataSize = waveInfo.SampleDataSize / sizeof(int16_t);
		int16_t *PCMData = reinterpret_cast<int16_t *>(waveData + 44);
		int32 ChunkSizeSamples = static_cast<int32>(SampleRate * LipSyncSequenceDuration);
		int32 ChunkSize = NumChannels * ChunkSizeSamples;
		int BufferSize = 4096;

		// Validate audio format
		if (NumChannels <= 0 || SampleRate <= 0)
		{
			ensureMsgf(false, TEXT("Invalid audio file format. NumChannels or SampleRate is not valid."));
			onFrameSequenceCooked.Broadcast(nullptr, false);
			return;
		}

		FString modelPath = UseOfflineModel ? FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("OVRLipSync"),
															  TEXT("OfflineModel"), TEXT("ovrlipsync_offline_model.pb"))
											: FString();

		Async(
			EAsyncExecution::Thread,
			[this, PCMData, PCMDataSize, ChunkSize, ChunkSizeSamples, NumChannels, modelPath, SampleRate, BufferSize]()
			{
				UOVRLipSyncFrameSequence *Sequence = NewObject<UOVRLipSyncFrameSequence>();
				UOVRLipSyncContextWrapper context(ovrLipSyncContextProvider_Enhanced, SampleRate, BufferSize,
												  modelPath);

				TArray<float> CurrentVisemes;
				TArray<TArray<float>> PreviousFrameBuffer;
				float LaughterScore = 0.0f;
				int32_t FrameDelayInMs = 0;

				std::map<int, int> VisemeDurations; // Tracks phoneme durations for tempo analysis
				int TotalFrames = 0;

				// Number of frames used for interpolation, adjusted dynamically based on speech tempo
				int NumInterpolationFrames = MaxInterpolationFrames;
				NumInterpolationFrames = FMath::Clamp(NumInterpolationFrames, 1, MaxInterpolationFrames);

				// Process audio chunks to generate phoneme data
				for (int offs = 0; offs + ChunkSize < PCMDataSize; offs += ChunkSize)
				{
					context.ProcessFrame(PCMData + offs, ChunkSizeSamples, CurrentVisemes, LaughterScore,
										 FrameDelayInMs, NumChannels > 1);

					TotalFrames++;

					// Analyze phoneme durations for dynamic interpolation adjustment
					for (int i = 0; i < CurrentVisemes.Num(); ++i)
					{
						if (CurrentVisemes[i] > 0.5f)
						{
							VisemeDurations[i]++;
						}
					}

					// Interpolate phoneme data for smoother transitions
					if (bEnableInterpolation && PreviousFrameBuffer.Num() > 0)
					{
						TArray<float> SmoothedVisemes;
						for (int i = 0; i < CurrentVisemes.Num(); ++i)
						{
							float WeightedSum = CurrentVisemes[i];
							float TotalWeight = 1.0f;

							// Include weighted previous frames
							for (int j = 0; j < PreviousFrameBuffer.Num(); ++j)
							{
								float Weight = 1.0f - static_cast<float>(j + 1) / (NumInterpolationFrames + 1);
								WeightedSum += PreviousFrameBuffer[j][i] * Weight;
								TotalWeight += Weight;
							}

							float SmoothedValue = WeightedSum / TotalWeight;
							SmoothedVisemes.Add(SmoothedValue);
						}
						Sequence->Add(SmoothedVisemes, LaughterScore);
					}
					else
					{
						Sequence->Add(CurrentVisemes, LaughterScore);
					}

					// Update the buffer of previous frames
					PreviousFrameBuffer.Insert(CurrentVisemes, 0);
					if (PreviousFrameBuffer.Num() > NumInterpolationFrames)
					{
						PreviousFrameBuffer.RemoveAt(PreviousFrameBuffer.Num() - 1);
					}
				}

				// Analyze average phoneme duration to determine speech tempo
				float AverageVisemeDuration = 0.0f;
				for (const auto &Pair : VisemeDurations)
				{
					AverageVisemeDuration += Pair.second;
				}
				AverageVisemeDuration /= VisemeDurations.size();

				// Adjust interpolation frames dynamically based on tempo
				if (AverageVisemeDuration < 6)
				{
					NumInterpolationFrames =
						FMath::Clamp(static_cast<int>(NumInterpolationFrames * 0.5f), 1, MaxInterpolationFrames);
				}
				else if (AverageVisemeDuration > 9)
				{
					NumInterpolationFrames =
						FMath::Clamp(static_cast<int>(NumInterpolationFrames * 1.5f), 1, MaxInterpolationFrames);
				}

				// Broadcast results back to the game thread
				AsyncTask(ENamedThreads::GameThread,
						  [Sequence, this]() { onFrameSequenceCooked.Broadcast(Sequence, true); });
			});
	}
	else
		onFrameSequenceCooked.Broadcast(nullptr, false);
}
