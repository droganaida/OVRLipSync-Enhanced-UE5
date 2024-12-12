// Frame Sequence Maker fork by Aida Drogan based on CookFrameSequenceAsync by IlgarLunin https://github.com/IlgarLunin/UE4OVRLipSyncCookFrameSequence

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OVRLipSyncContextWrapper.h"
#include "OVRLipSyncFrame.h"
#include "CookFrameSequenceAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrameSequenceCoocked, UOVRLipSyncFrameSequence *, FrameSequence, bool,
											 Success);
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
	static UCookFrameSequenceAsync *CookFrameSequence(const TArray<uint8> &RawSamples, bool UseOfflineModel = false,
													  bool bEnableInterpolation = true,
													  int MaxInterpolationFrames = 12);

	TArray<uint8> RawSamples;
	bool UseOfflineModel;
	bool bEnableInterpolation;
	int MaxInterpolationFrames;

	virtual void Activate() override;
	
};
