#include "SavaSettingsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Overlay.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"

USavaSettingsWidget::USavaSettingsWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	GeneralHelpTitle = NSLOCTEXT("SavaSettings", "AimTitle", "感度の調整");
	GeneralHelpBody = NSLOCTEXT("SavaSettings", "AimHelp", "小さい値：精密な照準\n大きい値：素早い振り向き\n\n数値をクリックすると、直接入力できます。\n\n変更は「設定を適用」で保存されます。");
	VideoHelpTitle = NSLOCTEXT("SavaSettings", "VideoTitle", "画面の調整");
	VideoHelpBody = NSLOCTEXT("SavaSettings", "VideoHelp", "ウィンドウの大きさは、解像度で変更できます。\n\n変更後は15秒以内に確認してください。確認しない場合、元の表示に戻ります。");
	PendingMessage = NSLOCTEXT("SavaSettings", "Pending", "● 未適用の変更があります");
	SavedMessage = NSLOCTEXT("SavaSettings", "Saved", "変更はすべて適用済みです");
	PreviewHint = NSLOCTEXT("SavaSettings", "Preview", "画面変更後は、15秒以内に「維持する」を選んでください。");
	LockedHint = NSLOCTEXT("SavaSettings", "Locked", "画面モードと解像度は、スタンドアロンゲームで変更できます。");
	CountdownFormat = NSLOCTEXT("SavaSettings", "Countdown", "{Seconds} 秒後に変更前の設定へ戻ります。");
}

void USavaSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!Pages) return;
	GeneralTab->OnClicked.AddUniqueDynamic(this, &ThisClass::GeneralClicked);
	VideoTab->OnClicked.AddUniqueDynamic(this, &ThisClass::VideoClicked);
	CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseClicked);
	ApplyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplyClicked);
	ResetButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetClicked);
	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmClicked);
	RevertButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RevertClicked);
	SensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SensitivityChanged);
	SensitivityInput->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SensitivityChanged);
	SensitivityInput->OnValueCommitted.AddUniqueDynamic(this, &ThisClass::SensitivityCommitted);
	FrameLimitInput->OnValueChanged.AddUniqueDynamic(this, &ThisClass::FrameLimitChanged);
	FrameLimitInput->OnValueCommitted.AddUniqueDynamic(this, &ThisClass::FrameLimitCommitted);
	DisplayModeInput->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::ModeChanged);
	ResolutionInput->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::ResolutionChanged);
	InvertOff->OnClicked.AddUniqueDynamic(this, &ThisClass::InvertOffClicked);
	InvertOn->OnClicked.AddUniqueDynamic(this, &ThisClass::InvertOnClicked);
	VSyncOff->OnClicked.AddUniqueDynamic(this, &ThisClass::VSyncOffClicked);
	VSyncOn->OnClicked.AddUniqueDynamic(this, &ThisClass::VSyncOnClicked);
}

void USavaSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bHasPresented = false;
	Present(Presentation);
	SelectTab(ActiveTab);
	if (Intro) PlayAnimation(Intro);
}

void USavaSettingsWidget::Present(const FSavaSettingsPresentation& NewPresentation)
{
	const bool bResolutionsChanged = Presentation.Resolutions != NewPresentation.Resolutions;
	const bool bSelectionChanged = !bHasPresented || Presentation.bDirty != NewPresentation.bDirty
		|| Presentation.Values.bInvertY != NewPresentation.Values.bInvertY || Presentation.Values.bVSync != NewPresentation.Values.bVSync;
	const bool bCountdownChanged = !bHasPresented || Presentation.SecondsRemaining != NewPresentation.SecondsRemaining;
	Presentation = NewPresentation;
	if (!Pages) return;
	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	if (!FMath::IsNearlyEqual(SensitivitySlider->GetValue(), Presentation.Values.MouseSensitivity)) SensitivitySlider->SetValue(Presentation.Values.MouseSensitivity);
	if (!FMath::IsNearlyEqual(SensitivityInput->GetValue(), Presentation.Values.MouseSensitivity)) SensitivityInput->SetValue(Presentation.Values.MouseSensitivity);
	if (!FMath::IsNearlyEqual(FrameLimitInput->GetValue(), Presentation.Values.FrameRateLimit)) FrameLimitInput->SetValue(Presentation.Values.FrameRateLimit);
	if (DisplayModeInput->GetOptionCount() == 0)
	{
		DisplayModeInput->AddOption(TEXT("ウィンドウ"));
		DisplayModeInput->AddOption(TEXT("ボーダーレス"));
		DisplayModeInput->AddOption(TEXT("フルスクリーン"));
	}
	DisplayModeInput->SetSelectedIndex(static_cast<int32>(Presentation.Values.DisplayMode));
	if (bResolutionsChanged || ResolutionInput->GetOptionCount() != Presentation.Resolutions.Num())
	{
		ResolutionInput->ClearOptions();
		for (FIntPoint Size : Presentation.Resolutions)
			ResolutionInput->AddOption(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y));
	}
	ResolutionInput->SetSelectedIndex(Presentation.Resolutions.IndexOfByKey(Presentation.Values.Resolution));
	DisplayModeInput->SetIsEnabled(Presentation.bCanChangeDisplay);
	ResolutionInput->SetIsEnabled(Presentation.bCanChangeDisplay && Presentation.Values.DisplayMode != ESavaDisplayMode::Borderless);
	ApplyButton->SetIsEnabled(Presentation.bDirty);
	StatusText->SetText(!Presentation.Status.IsEmpty() ? FText::FromString(Presentation.Status)
		: Presentation.bDirty ? PendingMessage : SavedMessage);
	if (bSelectionChanged) UpdateStateVisuals(Presentation.Values.bInvertY, Presentation.Values.bVSync, Presentation.bDirty);
	ConfirmOverlay->SetVisibility(Presentation.bConfirmingDisplay ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SettingsBody->SetIsEnabled(!Presentation.bConfirmingDisplay);
	if (bCountdownChanged) CountdownText->SetText(FText::Format(CountdownFormat, FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(Presentation.SecondsRemaining)}}));
	DisplayHint->SetText(Presentation.bCanChangeDisplay ? PreviewHint : LockedHint);
	bHasPresented = true;
}

void USavaSettingsWidget::SelectTab(int32 Index)
{
	ActiveTab = FMath::Clamp(Index, 0, 1);
	if (!Pages) return;
	UpdateTabVisuals(ActiveTab);
}

void USavaSettingsWidget::Emit(ESavaSettingsField Field, double Value) { if (!bRefreshing) OnSettingEdited.ExecuteIfBound(Field, Value); }
void USavaSettingsWidget::GeneralClicked() { SelectTab(0); }
void USavaSettingsWidget::VideoClicked() { SelectTab(1); }
void USavaSettingsWidget::CloseClicked() { OnCloseRequested.ExecuteIfBound(); }
void USavaSettingsWidget::ApplyClicked() { OnApplyRequested.ExecuteIfBound(); }
void USavaSettingsWidget::ResetClicked() { OnResetRequested.ExecuteIfBound(); }
void USavaSettingsWidget::ConfirmClicked() { OnConfirmRequested.ExecuteIfBound(); }
void USavaSettingsWidget::RevertClicked() { OnRevertRequested.ExecuteIfBound(); }
void USavaSettingsWidget::SensitivityChanged(float Value) { Emit(ESavaSettingsField::Sensitivity, Value); }
void USavaSettingsWidget::SensitivityCommitted(float Value, ETextCommit::Type) { SensitivityChanged(Value); }
void USavaSettingsWidget::FrameLimitChanged(float Value) { Emit(ESavaSettingsField::FrameLimit, Value); }
void USavaSettingsWidget::FrameLimitCommitted(float Value, ETextCommit::Type) { FrameLimitChanged(Value); }
void USavaSettingsWidget::ModeChanged(FString Option, ESelectInfo::Type) { Emit(ESavaSettingsField::DisplayMode, DisplayModeInput->FindOptionIndex(Option)); }
void USavaSettingsWidget::ResolutionChanged(FString Option, ESelectInfo::Type) { Emit(ESavaSettingsField::Resolution, ResolutionInput->FindOptionIndex(Option)); }
void USavaSettingsWidget::InvertOffClicked() { Emit(ESavaSettingsField::InvertY, 0); }
void USavaSettingsWidget::InvertOnClicked() { Emit(ESavaSettingsField::InvertY, 1); }
void USavaSettingsWidget::VSyncOffClicked() { Emit(ESavaSettingsField::VSync, 0); }
void USavaSettingsWidget::VSyncOnClicked() { Emit(ESavaSettingsField::VSync, 1); }

FReply USavaSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat())
		{
			if (Presentation.bConfirmingDisplay) OnRevertRequested.ExecuteIfBound();
			else OnCloseRequested.ExecuteIfBound();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
