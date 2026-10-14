// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Lobby/SavaInteractable.h"
#include "SavaInteractTerminal.generated.h"

class USavaTerminalWidget;

//近づいてしゃがみキーを押すと画面(メニュー)を開く端末(ロードアウトのクレート・マッチングの端末・ルーム設定の端末)
//・開く画面は MenuWidgetClass で選ぶ(USavaTerminalWidget を親にした Widget Blueprint)
//・範囲・案内・ホスト限定は親の ASavaInteractable が受け持つ
//・自分の画面だけの処理(サーバーとは関係しない)
UCLASS()
class SAVA_API ASavaInteractTerminal : public ASavaInteractable
{
	GENERATED_BODY()

public:
	//画面を開く(範囲内でしゃがみキーを押すと呼ばれる)
	UFUNCTION(BlueprintCallable, Category = "Sava|Terminal")
	void OpenMenu(APlayerController* PlayerController);

	//画面を閉じて操作を戻す(画面の「閉じる」ボタン・Esc から呼ぶ)
	UFUNCTION(BlueprintCallable, Category = "Sava|Terminal")
	void CloseMenu();

	UFUNCTION(BlueprintPure, Category = "Sava|Terminal")
	bool IsMenuOpen() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnInteract(APlayerController* PlayerController) override;
	virtual bool ShouldShowPrompt() const override { return !IsMenuOpen(); }

	//開く画面
	UPROPERTY(EditAnywhere, Category = "Sava|Terminal")
	TSubclassOf<USavaTerminalWidget> MenuWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<USavaTerminalWidget> MenuWidget;

	//画面を開いた人
	TWeakObjectPtr<APlayerController> MenuOwner;

	//画面を開いているか(OpenMenu で立て、CloseMenu で下ろす)
	bool bMenuOpen = false;
};
