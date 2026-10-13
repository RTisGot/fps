// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SavaTerminalWidget.generated.h"

class ASavaInteractTerminal;

//端末(ASavaInteractTerminal)で開く画面の親クラス
//Widget Blueprint の親をこれにすると、「閉じる」ボタンから Close Terminal を呼ぶだけで閉じられる。Esc キーでも閉じる
UCLASS(Abstract)
class SAVA_API USavaTerminalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//この画面を開いた端末
	UFUNCTION(BlueprintPure, Category = "Sava|Terminal")
	ASavaInteractTerminal* GetTerminal() const;

	//画面を閉じて操作を戻す
	UFUNCTION(BlueprintCallable, Category = "Sava|Terminal")
	void CloseTerminal();

	void SetTerminal(ASavaInteractTerminal* InTerminal);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<ASavaInteractTerminal> Terminal;
};
