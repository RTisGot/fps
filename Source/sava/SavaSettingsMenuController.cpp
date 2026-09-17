#include "SavaSettingsMenuController.h"
#include "SavaGameUserSettings.h"
#include "SavaSettingsWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreDelegates.h"

namespace
{
	EWindowMode::Type ToWindowMode(ESavaDisplayMode Mode)
	{
		return Mode == ESavaDisplayMode::Fullscreen ? EWindowMode::Fullscreen
			: Mode == ESavaDisplayMode::Borderless ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
	}
}

void USavaSettingsMenuController::Initialize(USavaGameUserSettings* InSettings, bool bAllowDisplayChanges)
{
	check(InSettings);
	Settings = InSettings;
	// Cancel video previews before UGameEngine saves settings on application exit.
	FCoreDelegates::OnPreExit.RemoveAll(this);
	FCoreDelegates::OnPreExit.AddUObject(this, &ThisClass::Discard);
	bCanChangeDisplay = bAllowDisplayChanges;
	Committed = Pending = ReadSettings();
	DesktopResolution = Settings->GetDesktopResolution();
	if (DesktopResolution.X <= 0 || DesktopResolution.Y <= 0) DesktopResolution = FIntPoint(1920, 1080);
	Resolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440)})
	{
		if (Size.X <= DesktopResolution.X && Size.Y <= DesktopResolution.Y) Resolutions.AddUnique(Size);
	}
	Resolutions.RemoveAll([](const FIntPoint& Size) { return Size.X < 800 || Size.Y < 600; });
	Resolutions.AddUnique(DesktopResolution);
	Resolutions.AddUnique(Pending.Resolution);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	Status.Reset();
	Publish();
}

void USavaSettingsMenuController::AttachView(USavaSettingsWidget* InView)
{
	View = InView;
	InView->OnSettingEdited.BindUObject(this, &ThisClass::Edit);
	InView->OnApplyRequested.BindUObject(this, &ThisClass::Apply);
	InView->OnResetRequested.BindUObject(this, &ThisClass::ResetToDefaults);
	InView->OnConfirmRequested.BindUObject(this, &ThisClass::ConfirmDisplay);
	InView->OnRevertRequested.BindUObject(this, &ThisClass::RevertDisplay);
	Publish();
}

FSavaSettingsValues USavaSettingsMenuController::ReadSettings() const
{
	FSavaSettingsValues Values;
	Values.MouseSensitivity = Settings->GetMouseSensitivity();
	Values.bInvertY = Settings->IsYInverted();
	Values.Resolution = Settings->GetScreenResolution();
	if (Values.Resolution.X <= 0 || Values.Resolution.Y <= 0) Values.Resolution = FIntPoint(1280, 720);
	Values.DisplayMode = Settings->GetFullscreenMode() == EWindowMode::Fullscreen ? ESavaDisplayMode::Fullscreen
		: Settings->GetFullscreenMode() == EWindowMode::WindowedFullscreen ? ESavaDisplayMode::Borderless : ESavaDisplayMode::Windowed;
	Values.FrameRateLimit = Settings->GetFrameRateLimit();
	Values.bVSync = Settings->IsVSyncEnabled();
	return Values;
}

void USavaSettingsMenuController::Edit(ESavaSettingsField Field, double Value)
{
	if (bConfirmingDisplay || !FMath::IsFinite(Value)) return;
	switch (Field)
	{
	case ESavaSettingsField::Sensitivity: Pending.MouseSensitivity = FMath::Clamp(static_cast<float>(Value), 0.1f, 3.0f); break;
	case ESavaSettingsField::InvertY: Pending.bInvertY = Value != 0; break;
	case ESavaSettingsField::DisplayMode:
		if (bCanChangeDisplay && Value >= 0 && Value <= 2)
		{
			Pending.DisplayMode = static_cast<ESavaDisplayMode>(static_cast<int32>(Value));
			if (Pending.DisplayMode == ESavaDisplayMode::Borderless) Pending.Resolution = DesktopResolution;
		}
		break;
	case ESavaSettingsField::Resolution:
		if (bCanChangeDisplay && Pending.DisplayMode != ESavaDisplayMode::Borderless && Value >= 0 && Value < Resolutions.Num())
			Pending.Resolution = Resolutions[static_cast<int32>(Value)];
		break;
	case ESavaSettingsField::FrameLimit: Pending.FrameRateLimit = Value <= 0 ? 0 : FMath::Clamp(static_cast<float>(Value), 30.0f, 500.0f); break;
	case ESavaSettingsField::VSync: Pending.bVSync = Value != 0; break;
	}
	Status.Reset();
	Publish();
}

void USavaSettingsMenuController::WriteSettings(const FSavaSettingsValues& Values)
{
	Settings->SetInputPreferences(Values.MouseSensitivity, Values.bInvertY);
	Settings->SetFrameRateLimit(Values.FrameRateLimit);
	Settings->SetVSyncEnabled(Values.bVSync);
	if (bCanChangeDisplay)
	{
		Settings->SetFullscreenMode(ToWindowMode(Values.DisplayMode));
		Settings->SetScreenResolution(Values.Resolution);
	}
}

void USavaSettingsMenuController::Apply()
{
	if (!Settings || !IsDirty() || bConfirmingDisplay) return;
	const bool bDisplayChanged = bCanChangeDisplay && (Pending.Resolution != Committed.Resolution || Pending.DisplayMode != Committed.DisplayMode);
	WriteSettings(Pending);
	Settings->ApplyNonResolutionSettings();
	if (bDisplayChanged)
	{
		// Keep disk settings unchanged until the player confirms the new mode.
		bConfirmingDisplay = true;
		ConfirmationDeadline = FPlatformTime::Seconds() + 15.0;
		ConfirmationTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickConfirmation), 0.25f);
		Settings->ApplyResolutionSettings(false);
	}
	else
	{
		Settings->SaveSettings();
		Committed = Pending = ReadSettings();
		Status = TEXT("設定を適用しました");
	}
	Publish();
}

void USavaSettingsMenuController::ConfirmDisplay()
{
	if (!bConfirmingDisplay) return;
	StopConfirmationTimer();
	bConfirmingDisplay = false;
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();
	Committed = Pending = ReadSettings();
	Status = TEXT("画面設定を保存しました");
	Publish();
}

void USavaSettingsMenuController::RevertDisplay()
{
	if (!bConfirmingDisplay) return;
	StopConfirmationTimer();
	bConfirmingDisplay = false;
	WriteSettings(Committed);
	Settings->ApplyResolutionSettings(false);
	Settings->ApplyNonResolutionSettings();
	Pending = Committed;
	Status = TEXT("変更前の設定に戻しました");
	Publish();
}

void USavaSettingsMenuController::Discard()
{
	RevertDisplay();
	Pending = Committed;
	Status.Reset();
	Publish();
}

void USavaSettingsMenuController::ResetToDefaults()
{
	if (bConfirmingDisplay) return;
	const FSavaSettingsValues Defaults;
	Pending = Defaults;
	if (!bCanChangeDisplay)
	{
		Pending.DisplayMode = Committed.DisplayMode;
		Pending.Resolution = Committed.Resolution;
	}
	Status = TEXT("初期値を選択しました。「適用」で反映されます");
	Publish();
}

bool USavaSettingsMenuController::TickConfirmation(float)
{
	if (!bConfirmingDisplay) return false;
	if (FPlatformTime::Seconds() >= ConfirmationDeadline)
	{
		// The ticker removes itself on return; do not remove it twice.
		ConfirmationTicker.Reset();
		RevertDisplay();
		return false;
	}
	Publish();
	return true;
}

void USavaSettingsMenuController::StopConfirmationTimer()
{
	if (ConfirmationTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(ConfirmationTicker);
	ConfirmationTicker.Reset();
}

void USavaSettingsMenuController::BeginDestroy()
{
	FCoreDelegates::OnPreExit.RemoveAll(this);
	StopConfirmationTimer();
	Super::BeginDestroy();
}

void USavaSettingsMenuController::Publish()
{
	if (USavaSettingsWidget* Widget = View.Get())
	{
		FSavaSettingsPresentation State;
		State.Values = Pending;
		State.Resolutions = Resolutions;
		State.bDirty = IsDirty();
		State.bCanChangeDisplay = bCanChangeDisplay;
		State.bConfirmingDisplay = bConfirmingDisplay;
		State.SecondsRemaining = bConfirmingDisplay ? FMath::Max(0, FMath::CeilToInt(ConfirmationDeadline - FPlatformTime::Seconds())) : 0;
		State.Status = Status;
		Widget->Present(State);
	}
}
