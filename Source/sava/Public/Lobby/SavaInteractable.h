// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SavaInteractable.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UUserWidget;

//近づいてしゃがみキーを押すと何かが起きる物の親クラス(端末 ASavaInteractTerminal・ボタン ASavaInteractButton)
//・範囲(InteractArea)に入っている間は案内(PromptWidgetClass)を出す
//・bHostOnly にすると部屋のホストだけが使える。ホスト以外には HostOnlyPromptWidgetClass(「ホストのみ」など)を出し、
//  しゃがみキーは普通のしゃがみになる
//・押したことを受け取るのは自分の画面だけ。サーバーに関わる処理は ASavaPlayerController の Server〜 を通して頼む
UCLASS(Abstract)
class SAVA_API ASavaInteractable : public AActor
{
	GENERATED_BODY()

public:
	ASavaInteractable();

	//範囲内でしゃがみキーを押すと AsavaCharacter が呼ぶ。使えないときは何もせず false を返す(キャラクターは普通にしゃがむ)
	bool TryInteract(APlayerController* PlayerController);

	//このプレイヤーが使えるか(ホスト限定の確認など)
	UFUNCTION(BlueprintPure, Category = "Sava|Interact")
	virtual bool CanInteract(const APlayerController* PlayerController) const;

	//ホストだけが使えるか
	UFUNCTION(BlueprintPure, Category = "Sava|Interact")
	virtual bool IsHostOnly() const { return bHostOnly; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//使われたときの処理(子クラスで書く。CanInteract を通った後に呼ばれる)
	virtual void OnInteract(APlayerController* PlayerController) {}

	//案内を出してよいか(端末は画面を開いている間は出さない)
	virtual bool ShouldShowPrompt() const { return true; }

	//案内を出し直す(まだ範囲内にいれば出し、いなければ消す)
	void RefreshPrompt(APlayerController* PlayerController);

	void HidePrompt();

	//見た目(クレート・端末・ボタンのメッシュを入れる)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Interact")
	TObjectPtr<UStaticMeshComponent> Mesh;

	//この箱に入っている間、しゃがみキーで使える
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Interact")
	TObjectPtr<UBoxComponent> InteractArea;

	//部屋のホストだけが使えるようにする
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Interact")
	bool bHostOnly = false;

	//範囲に入っている間に出す案内(空なら出さない)
	UPROPERTY(EditAnywhere, Category = "Sava|Interact")
	TSubclassOf<UUserWidget> PromptWidgetClass;

	//ホスト限定のとき、ホスト以外に出す案内(「ホストのみ」など。空なら何も出さない)
	UPROPERTY(EditAnywhere, Category = "Sava|Interact")
	TSubclassOf<UUserWidget> HostOnlyPromptWidgetClass;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PromptWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HostOnlyPromptWidget;
};
