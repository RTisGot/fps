// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Lobby/SavaInteractable.h"
#include "SavaInteractButton.generated.h"

//ボタンを押したときにすること
UENUM(BlueprintType)
enum class ESavaInteractButtonAction : uint8
{
	//何もしない(Blueprint の On Pressed で処理を書く)
	Custom,
	//TeamId のチームに入る
	JoinTeam,
	//準備 OK を切り替える
	ToggleReady,
	//部屋を抜けてソロロビーへ戻る
	LeaveRoom,
	//試合を始める(いつもホスト限定)
	StartMatch,
};

//マップに置いて、近づいてしゃがみキーで押すボタン
//・Action を選ぶだけで、チーム切替・準備 OK・退出・ゲーム開始になる
//・押したことは自分の画面で受け取り、サーバーに関わるものは ASavaPlayerController の Server〜 で頼む
//・音や光り方などの演出は Blueprint の On Pressed で付ける(押した本人の画面だけで呼ばれる)
UCLASS()
class SAVA_API ASavaInteractButton : public ASavaInteractable
{
	GENERATED_BODY()

public:
	virtual bool IsHostOnly() const override;

	//準備 OK はホストには要らないので、ホストは押せない(案内も出ず、しゃがみキーは普通のしゃがみになる)
	virtual bool CanInteract(const APlayerController* PlayerController) const override;

	UFUNCTION(BlueprintPure, Category = "Sava|Button")
	ESavaInteractButtonAction GetAction() const { return Action; }

protected:
	virtual void OnInteract(APlayerController* PlayerController) override;

	//押したときに呼ばれる(押した本人の画面だけ。Action の処理を頼んだ後)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Button", meta = (DisplayName = "On Pressed"))
	void K2_OnPressed(APlayerController* PlayerController);

	//押したときにすること
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Button")
	ESavaInteractButtonAction Action = ESavaInteractButtonAction::Custom;

	//JoinTeam で入るチーム(0 から)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Button", meta = (EditCondition = "Action == ESavaInteractButtonAction::JoinTeam", EditConditionHides))
	uint8 TeamId = 0;
};
