// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SavaInteractTerminal.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UUserWidget;
class USavaTerminalWidget;

//近づいてしゃがみキーを押すと画面(メニュー)を開く端末(ソロロビーのロードアウトのクレート・マッチングの端末)
//・開く画面は MenuWidgetClass で選ぶ(USavaTerminalWidget を親にした Widget Blueprint)
//・範囲(InteractArea)に入っている間は PromptWidgetClass の画面(「しゃがみで操作」など)を出す
//・自分の画面だけの処理(サーバーとは関係しない)
UCLASS()
class SAVA_API ASavaInteractTerminal : public AActor
{
	GENERATED_BODY()

public:
	ASavaInteractTerminal();

	//画面を開く(範囲内でしゃがみキーを押すと AsavaCharacter が呼ぶ)
	UFUNCTION(BlueprintCallable, Category = "Sava|Terminal")
	void OpenMenu(APlayerController* PlayerController);

	//画面を閉じて操作を戻す(画面の「閉じる」ボタン・Esc から呼ぶ)
	UFUNCTION(BlueprintCallable, Category = "Sava|Terminal")
	void CloseMenu();

	UFUNCTION(BlueprintPure, Category = "Sava|Terminal")
	bool IsMenuOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//見た目(クレート・端末のメッシュを入れる)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Terminal")
	TObjectPtr<UStaticMeshComponent> Mesh;

	//この箱に入っている間、しゃがみキーで画面を開ける
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Terminal")
	TObjectPtr<UBoxComponent> InteractArea;

	//開く画面
	UPROPERTY(EditAnywhere, Category = "Sava|Terminal")
	TSubclassOf<USavaTerminalWidget> MenuWidgetClass;

	//範囲に入っている間に出す案内(空なら出さない)
	UPROPERTY(EditAnywhere, Category = "Sava|Terminal")
	TSubclassOf<UUserWidget> PromptWidgetClass;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void SetPromptVisible(APlayerController* PlayerController, bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<USavaTerminalWidget> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PromptWidget;

	//画面を開いた人
	TWeakObjectPtr<APlayerController> MenuOwner;
};
