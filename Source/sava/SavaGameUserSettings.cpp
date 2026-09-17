// Copyright Epic Games, Inc. All Rights Reserved.

#include "SavaGameUserSettings.h"

USavaGameUserSettings* USavaGameUserSettings::GetSavaGameUserSettings()
{
	return Cast<USavaGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void USavaGameUserSettings::SetMouseSensitivity(const float NewSensitivity)
{
	const float ClampedSensitivity = FMath::IsFinite(NewSensitivity) ? FMath::Clamp(NewSensitivity, 0.1f, 3.0f) : 1.0f;

	if (FMath::IsNearlyEqual(MouseSensitivity, ClampedSensitivity))
	{
		return;
	}

	MouseSensitivity = ClampedSensitivity;
	SaveSettings();
}

void USavaGameUserSettings::SetInputPreferences(float NewSensitivity, bool bNewInvertY)
{
	MouseSensitivity = FMath::IsFinite(NewSensitivity) ? FMath::Clamp(NewSensitivity, 0.1f, 3.0f) : 1.0f;
	bInvertY = bNewInvertY;
}

void USavaGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	SetInputPreferences(MouseSensitivity, bInvertY);
}
