#pragma once

#include "CoreMinimal.h"
#include "SavaSettingsTypes.generated.h"

UENUM(BlueprintType)
enum class ESavaDisplayMode : uint8
{
	Windowed,
	Borderless,
	Fullscreen
};

/** Editable values. The menu controller owns the canonical Pending copy. */
USTRUCT(BlueprintType)
struct FSavaSettingsValues
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) float MouseSensitivity = 1.0f;
	UPROPERTY(BlueprintReadOnly) bool bInvertY = false;
	UPROPERTY(BlueprintReadOnly) ESavaDisplayMode DisplayMode = ESavaDisplayMode::Windowed;
	UPROPERTY(BlueprintReadOnly) FIntPoint Resolution = FIntPoint(1280, 720);
	UPROPERTY(BlueprintReadOnly) float FrameRateLimit = 144.0f;
	UPROPERTY(BlueprintReadOnly) bool bVSync = false;

	bool Equals(const FSavaSettingsValues& Other) const
	{
		return FMath::IsNearlyEqual(MouseSensitivity, Other.MouseSensitivity)
			&& bInvertY == Other.bInvertY && DisplayMode == Other.DisplayMode
			&& Resolution == Other.Resolution
			&& FMath::IsNearlyEqual(FrameRateLimit, Other.FrameRateLimit) && bVSync == Other.bVSync;
	}
};

UENUM(BlueprintType)
enum class ESavaSettingsField : uint8 { Sensitivity, InvertY, DisplayMode, Resolution, FrameLimit, VSync };

/** Read-only presentation snapshot; never used to apply or save settings. */
USTRUCT(BlueprintType)
struct FSavaSettingsPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FSavaSettingsValues Values;
	UPROPERTY(BlueprintReadOnly) TArray<FIntPoint> Resolutions;
	UPROPERTY(BlueprintReadOnly) bool bDirty = false;
	UPROPERTY(BlueprintReadOnly) bool bCanChangeDisplay = true;
	UPROPERTY(BlueprintReadOnly) bool bConfirmingDisplay = false;
	UPROPERTY(BlueprintReadOnly) int32 SecondsRemaining = 0;
	UPROPERTY(BlueprintReadOnly) FString Status;
};
