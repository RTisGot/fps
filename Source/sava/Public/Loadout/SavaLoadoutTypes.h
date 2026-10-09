// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SavaLoadoutTypes.generated.h"

class USavaGadgetData;
class USavaSkillData;
class USavaWeaponData;

//プレイヤーが選んだ装備の組(メイン武器・サブ武器・スキル・ガジェット)
//・自分の PC では USavaLoadoutSubsystem が持ち、ディスクに保存する
//・サーバーへは ASavaPlayerController::ServerSetLoadout で送り、ASavaPlayerState が持つ
//アセットの「場所(パス)」で持つ。通信・保存のどちらでもそのまま送れて、相手側で同じアセットを指せるため
USTRUCT(BlueprintType)
struct SAVA_API FSavaLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Loadout")
	TSoftObjectPtr<USavaWeaponData> PrimaryWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Loadout")
	TSoftObjectPtr<USavaWeaponData> SecondaryWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Loadout")
	TSoftObjectPtr<USavaSkillData> Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Loadout")
	TSoftObjectPtr<USavaGadgetData> Gadget;

	bool operator==(const FSavaLoadout& Other) const
	{
		return PrimaryWeapon == Other.PrimaryWeapon
			&& SecondaryWeapon == Other.SecondaryWeapon
			&& Skill == Other.Skill
			&& Gadget == Other.Gadget;
	}
	bool operator!=(const FSavaLoadout& Other) const { return !(*this == Other); }
};
