// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SavaPlayerController.generated.h"

class UUserWidget;
class USavaScoreboardWidget;
class USavaSettingsMenuController;

//プレイヤーの操作の窓口。設定画面(Esc)とスコアボード(Tab)をここで持つ
//キャラクターは死ぬと消えるが、PlayerController は残るので、リスポーンしても画面が開いたまま続く
UCLASS()
class SAVA_API ASavaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASavaPlayerController();

	//設定画面を開いているか(開いている間はキャラの操作を受け付けない)
	UFUNCTION(BlueprintPure, Category = "UI|Settings")
	bool IsSettingsMenuOpen() const;

	//設定画面を閉じる
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettingsMenu();

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Settings")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	/** スコアボードWidgetのクラス */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Scoreboard")
	TSubclassOf<USavaScoreboardWidget> ScoreboardWidgetClass;

private:
	void ToggleSettingsMenu();

	/** スコアボードを表示する */
	void ShowScoreboard();

	/** スコアボードを非表示にする */
	void HideScoreboard();

	//今操作しているキャラクターの操作を受け付けるか(キャラクターがいなければ何もしない)
	void SetPawnGameplayInputEnabled(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;//生成した設定画面Widgetへの参照を保持する変数

	UPROPERTY(Transient)
	TObjectPtr<USavaSettingsMenuController> SettingsMenuController;

	/** 現在表示しているスコアボード */
	UPROPERTY(Transient)
	TObjectPtr<USavaScoreboardWidget> ScoreboardWidget;
};
