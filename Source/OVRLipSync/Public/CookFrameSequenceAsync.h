// Frame Sequence Maker fork by Aida Drogan based on CookFrameSequenceAsync by IlgarLunin
// https://github.com/IlgarLunin/UE4OVRLipSyncCookFrameSequence

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OVRLipSyncContextWrapper.h"
#include "OVRLipSyncFrame.h"
#include "CookFrameSequenceAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrameSequenceCoocked, UOVRLipSyncFrameSequence *, FrameSequence, bool,
											 Success);

USTRUCT(BlueprintType)
struct FVisemeInterpolationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableInterpolation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxInterpolationFrames = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStrictConsonantLock = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LipSync", meta = (ClampMin = "1"))
	int32 MinHoldFrames = 2;
};

/**
 * Generates Frame Sequence for LipSync
 */
UCLASS()
class OVRLIPSYNC_API UCookFrameSequenceAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category = "LipSync")
	FFrameSequenceCoocked onFrameSequenceCooked;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "LipSync")
	static UCookFrameSequenceAsync *
	CookFrameSequence(const TArray<uint8> &RawSamples, bool UseOfflineModel = false,
					  const FVisemeInterpolationSettings &Settings = FVisemeInterpolationSettings());

	TArray<uint8> RawSamples;
	bool UseOfflineModel;
	FVisemeInterpolationSettings InterpolationSettings;

	virtual void Activate() override;
};
