/*******************************************************************************
 * Filename    :   AudioConverterLibrary.cpp
 * Content     :   Audio file processing and loading utilities
 * Modified by :   Aida Drogan, SilverCord-VR
 *
 * Modifications:
 * - Added function `ParseWavHeader` for extracting WAV file metadata dynamically.
 * - Enhanced file loading logic to fallback on default parameters if WAV header parsing fails.
 * - Improved error handling and compatibility with various WAV file formats.
 *
 * Licensed under standard Unreal Engine license.
 *******************************************************************************/

#include "AudioConverterLibrary.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"

TArray<uint8> UAudioConverterLibrary::LoadWaveFile(const FString &FilePath)
{
	TArray<uint8> FileData;

	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
		return TArray<uint8>();
	}

	if (FileData.Num() <= 44)
	{
		UE_LOG(LogTemp, Error, TEXT("File too short to be a valid WAV file: %s"), *FilePath);
		return TArray<uint8>();
	}
	return FileData;
}

void UAudioConverterLibrary::SetSoundFromDisk(UAudioComponent *AudioComponent, const FString &FilePath)
{
	// Load sound file data from disk
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load file from disk: %s"), *FilePath);
		return;
	}

	// Parse WAV header to get audio parameters
	int32 SampleRate;
	int32 NumChannels;
	int32 BitsPerSample;
	int32 HeaderSize; // Skipping the WAV header

	if (!ParseWavHeader(FileData, SampleRate, NumChannels, BitsPerSample, HeaderSize))
	{
		/*
		// Return logic if default headers no needed
		UE_LOG(LogTemp, Error, TEXT("Failed to parse WAV header: %s"), *FilePath);
		return;
		*/

		// Assuming the file is a 16-bit stereo WAV file at 44.1kHz
		SampleRate = 44100;
		NumChannels = 2;
		BitsPerSample = 16;
		HeaderSize = 44; 
	}



	if (FileData.Num() <= HeaderSize)
	{
		UE_LOG(LogTemp, Error, TEXT("File too short to be a valid WAV file: %s"), *FilePath);
		return;
	}

	// Create a USoundWaveProcedural object
	USoundWaveProcedural *SoundWave = NewObject<USoundWaveProcedural>();
	SoundWave->SetSampleRate(SampleRate);
	SoundWave->NumChannels = NumChannels;
	SoundWave->Duration = (FileData.Num() - HeaderSize) / (SampleRate * NumChannels * (BitsPerSample / 8.0f));
	SoundWave->SoundGroup = SOUNDGROUP_Default;

	// Skip the WAV header and add the PCM data to the SoundWave
	FileData.RemoveAt(0, HeaderSize);
	SoundWave->QueueAudio(FileData.GetData(), FileData.Num());

	// Set the sound for the AudioComponent using the SoundWave
	if (AudioComponent)
	{
		AudioComponent->SetSound(SoundWave);
	}
}

bool UAudioConverterLibrary::ParseWavHeader(const TArray<uint8> &FileData, int32 &SampleRate, int32 &NumChannels,
											int32 &BitsPerSample, int32 &HeaderSize)
{
	// Check if the file is long enough to contain the WAV header
	if (FileData.Num() <= 44)
	{
		return false;
	}

	// Extract audio parameters from the WAV header
	SampleRate = *(int32 *)&FileData[24];
	NumChannels = *(int16 *)&FileData[22];
	BitsPerSample = *(int16 *)&FileData[34];
	HeaderSize = 44; // Skipping the WAV header

	return true;
}