// Audio Converter Library © Aida Drogan | SilverCord-VR 2024

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sound/SoundWave.h"
#include "AudioConverterLibrary.generated.h"

/**
 * A library for loading audio files and setting sounds for AudioComponents.
 */
UCLASS()
class OVRLIPSYNC_API UAudioConverterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Load binary data from a WAV file.
	 *
	 * @param FilePath The path to the WAV file.
	 * @return The loaded binary data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio Converter")
	static TArray<uint8> LoadWaveFile(const FString &FilePath);

	/**
	 * Set the sound of an AudioComponent using data from a WAV file on disk.
	 *
	 * @param AudioComponent The AudioComponent to set the sound for.
	 * @param FilePath The path to the WAV file.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio Converter")
	static void SetSoundFromDisk(UAudioComponent *AudioComponent, const FString &FilePath);

private:
	/**
	 * Parse the WAV header to get audio parameters.
	 *
	 * @param FileData The binary data of the WAV file.
	 * @param SampleRate The sample rate of the audio.
	 * @param NumChannels The number of audio channels.
	 * @param BitsPerSample The number of bits per sample.
	 * @param HeaderSize The size of the WAV header.
	 * @return True if the header was parsed successfully, false otherwise.
	 */
	static bool ParseWavHeader(const TArray<uint8> &FileData, int32 &SampleRate, int32 &NumChannels,
							   int32 &BitsPerSample, int32 &HeaderSize);
};
