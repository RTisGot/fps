// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "SavaGameUserSettings.generated.h"

/** Owns and persists user-configurable game settings. */
UCLASS(Config = GameUserSettings)
class SAVA_API USavaGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** Returns the active settings object as the project-specific type. */
	UFUNCTION(BlueprintPure, Category = "Settings", meta = (DisplayName = "Get Sava Game User Settings"))
	static USavaGameUserSettings* GetSavaGameUserSettings();

	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	/** Updates and immediately persists mouse sensitivity. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Input")
	void SetMouseSensitivity(float NewSensitivity);

	bool IsYInverted() const { return bInvertY; }
	/** Used by the settings transaction; saving is a separate commit step. */
	void SetInputPreferences(float NewSensitivity, bool bNewInvertY);
	virtual void ValidateSettings() override;

private:
	UPROPERTY(Config)
	float MouseSensitivity = 1.0f;

	UPROPERTY(Config)
	bool bInvertY = false;
};
