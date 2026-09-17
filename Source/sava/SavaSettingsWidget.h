#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SavaSettingsTypes.h"
#include "SavaSettingsWidget.generated.h"

class UButton;
class UBorder;
class UOverlay;
class UVerticalBox;
class USlider;
class USpinBox;
class UComboBoxString;
class UTextBlock;
class UWidgetSwitcher;
class UWidgetAnimation;

DECLARE_DELEGATE_TwoParams(FSavaSettingsInput, ESavaSettingsField, double);

/** Thin view adapter. All layout, styling and animation live in WBP_SettingsMenu. */
UCLASS()
class SAVA_API USavaSettingsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	USavaSettingsWidget(const FObjectInitializer& Initializer);
	void Present(const FSavaSettingsPresentation& NewPresentation);
	FSavaSettingsInput OnSettingEdited;
	FSimpleDelegate OnApplyRequested, OnResetRequested, OnCloseRequested, OnConfirmRequested, OnRevertRequested;

protected:
	/** Implemented in WBP_SettingsMenu: tab layout, help copy and selection appearance. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|View")
	void UpdateTabVisuals(int32 TabIndex);

	/** Runs only when selection/dirty state changes, not on every countdown tick. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|View")
	void UpdateStateVisuals(bool bInvertY, bool bVSync, bool bDirty);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Appearance")
	FLinearColor SelectedTint = FLinearColor(0.38f, 1.0f, 0.82f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Appearance")
	FLinearColor IdleTint = FLinearColor(0.72f, 0.80f, 0.84f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText GeneralHelpTitle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text", meta = (MultiLine = "true")) FText GeneralHelpBody;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText VideoHelpTitle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text", meta = (MultiLine = "true")) FText VideoHelpBody;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText PendingMessage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText SavedMessage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText PreviewHint;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText LockedHint;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Text") FText CountdownFormat;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> GeneralTab;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> VideoTab;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UBorder> GeneralUnderline;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UBorder> VideoUnderline;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ApplyButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ResetButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> SensitivitySlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> SensitivityInput;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> FrameLimitInput;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> DisplayModeInput;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> ResolutionInput;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> InvertOff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> InvertOn;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> VSyncOff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> VSyncOn;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> HelpTitle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> HelpText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DisplayHint;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOverlay> ConfirmOverlay;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> SettingsBody;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RevertButton;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Intro;

private:
	UPROPERTY(BlueprintReadOnly, Category = "Settings|View", meta = (AllowPrivateAccess = "true"))
	FSavaSettingsPresentation Presentation;
	bool bHasPresented = false;
	bool bRefreshing = false;
	int32 ActiveTab = 0;
	void SelectTab(int32 Index);
	void Emit(ESavaSettingsField Field, double Value);
	UFUNCTION() void GeneralClicked();
	UFUNCTION() void VideoClicked();
	UFUNCTION() void CloseClicked();
	UFUNCTION() void ApplyClicked();
	UFUNCTION() void ResetClicked();
	UFUNCTION() void ConfirmClicked();
	UFUNCTION() void RevertClicked();
	UFUNCTION() void SensitivityChanged(float Value);
	UFUNCTION() void SensitivityCommitted(float Value, ETextCommit::Type CommitType);
	UFUNCTION() void FrameLimitChanged(float Value);
	UFUNCTION() void FrameLimitCommitted(float Value, ETextCommit::Type CommitType);
	UFUNCTION() void ModeChanged(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() void ResolutionChanged(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() void InvertOffClicked();
	UFUNCTION() void InvertOnClicked();
	UFUNCTION() void VSyncOffClicked();
	UFUNCTION() void VSyncOnClicked();
};
