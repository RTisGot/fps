#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "SavaSettingsTypes.h"
#include "SavaSettingsMenuController.generated.h"

class USavaGameUserSettings;
class USavaSettingsWidget;

/** Owns editing, validation, apply/cancel and display-confirmation lifecycle. */
UCLASS()
class SAVA_API USavaSettingsMenuController : public UObject
{
	GENERATED_BODY()
public:
	void Initialize(USavaGameUserSettings* InSettings, bool bAllowDisplayChanges);
	void AttachView(USavaSettingsWidget* InView);
	void Edit(ESavaSettingsField Field, double Value);
	void Apply();
	void ConfirmDisplay();
	void RevertDisplay();
	void Discard();
	void ResetToDefaults();
	const FSavaSettingsValues& GetPending() const { return Pending; }
	bool IsDirty() const { return !Pending.Equals(Committed); }
	virtual void BeginDestroy() override;

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	FSavaSettingsValues Pending;
	FSavaSettingsValues Committed;
	UPROPERTY(Transient) TObjectPtr<USavaGameUserSettings> Settings;
	TWeakObjectPtr<USavaSettingsWidget> View;
	TArray<FIntPoint> Resolutions;
	FIntPoint DesktopResolution = FIntPoint(1920, 1080);
	bool bCanChangeDisplay = false;
	bool bConfirmingDisplay = false;
	double ConfirmationDeadline = 0.0;
	FTSTicker::FDelegateHandle ConfirmationTicker;
	FString Status;

	FSavaSettingsValues ReadSettings() const;
	void WriteSettings(const FSavaSettingsValues& Values);
	void Publish();
	void StopConfirmationTimer();
	bool TickConfirmation(float DeltaTime);
};
