// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "SavaTeamPlayerStart.generated.h"

//チームごとのスポーン地点。レベルに各チーム 3 個以上置く(ラウンド開始時に全員が同時に出るため)
//CTF の GameMode は、自チームの地点のうち空いているものを選ぶ
UCLASS()
class SAVA_API ASavaTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	ASavaTeamPlayerStart(const FObjectInitializer& ObjectInitializer);

	uint8 GetTeamId() const { return TeamId; }

protected:
	//このスポーン地点を使うチーム(0 または 1)
	UPROPERTY(EditAnywhere, Category = "Sava|Team")
	uint8 TeamId = 0;
};
