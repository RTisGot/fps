#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../SavaGameUserSettings.h"
#include "../SavaSettingsMenuController.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaPendingSettingsTest, "Sava.Settings.PendingIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaPendingSettingsTest::RunTest(const FString&)
{
	USavaGameUserSettings* Storage = NewObject<USavaGameUserSettings>();
	Storage->SetInputPreferences(0.8f, false);
	Storage->SetScreenResolution(FIntPoint(1600, 900));
	Storage->SetFullscreenMode(EWindowMode::Windowed);
	Storage->SetFrameRateLimit(120);
	USavaSettingsMenuController* Menu = NewObject<USavaSettingsMenuController>();
	Menu->Initialize(Storage, false);
	TestFalse(TEXT("Opening is clean"), Menu->IsDirty());
	Menu->Edit(ESavaSettingsField::Sensitivity, 2.0);
	Menu->Edit(ESavaSettingsField::InvertY, 1);
	TestEqual(TEXT("Edits remain pending"), Storage->GetMouseSensitivity(), 0.8f);
	TestFalse(TEXT("Invert is not applied early"), Storage->IsYInverted());
	TestTrue(TEXT("Edits are dirty"), Menu->IsDirty());
	Menu->Discard();
	TestEqual(TEXT("Cancel restores applied sensitivity"), Menu->GetPending().MouseSensitivity, 0.8f);
	TestFalse(TEXT("Cancel clears dirty flag"), Menu->IsDirty());
	Menu->Edit(ESavaSettingsField::Sensitivity, 100);
	TestEqual(TEXT("Sensitivity is clamped"), Menu->GetPending().MouseSensitivity, 3.0f);
	Menu->Edit(ESavaSettingsField::Sensitivity, std::numeric_limits<double>::quiet_NaN());
	TestEqual(TEXT("NaN is ignored"), Menu->GetPending().MouseSensitivity, 3.0f);
	Menu->Edit(ESavaSettingsField::DisplayMode, 2);
	Menu->Edit(ESavaSettingsField::Resolution, 0);
	TestTrue(TEXT("PIE preserves display mode"), Menu->GetPending().DisplayMode == ESavaDisplayMode::Windowed);
	TestEqual(TEXT("PIE preserves resolution"), Menu->GetPending().Resolution, FIntPoint(1600, 900));
	Menu->ResetToDefaults();
	TestEqual(TEXT("Reset is staged"), Menu->GetPending().MouseSensitivity, 1.0f);
	TestEqual(TEXT("Reset does not modify applied sensitivity"), Storage->GetMouseSensitivity(), 0.8f);
	TestEqual(TEXT("Reset preserves PIE resolution"), Menu->GetPending().Resolution, FIntPoint(1600, 900));
	Menu->Discard();
	TestFalse(TEXT("Final discard clears pending reset"), Menu->IsDirty());
	return true;
}
#endif
