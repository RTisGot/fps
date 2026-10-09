// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaWeaponData.generated.h"

class UAnimMontage;
class USkeletalMesh;
class UStaticMesh;
class USoundBase;
class UTexture2D;

//武器 1 種類分のデータ(データアセット)
//新しい武器は、これを 1 つ作って数値と見た目を入れるだけで追加できる(コンテンツブラウザ → Miscellaneous → Data Asset → SavaWeaponData)
UCLASS(BlueprintType, Const)
class SAVA_API USavaWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//--------------------------------基本

	//画面に出す名前
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	//選択画面などに出すアイコン
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	ESavaWeaponType WeaponType = ESavaWeaponType::Rifle;

	//どちらの枠の武器か(サブ武器は予備弾が無限)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	ESavaWeaponSlot Slot = ESavaWeaponSlot::Primary;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	ESavaFireMode FireMode = ESavaFireMode::FullAuto;

	//カスタムを付ける前の数値
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ShowOnlyInnerProperties))
	FSavaWeaponStats Stats;

	//--------------------------------見た目・音(自分の画面だけ)

	//1 人称の腕に持たせるskeletalメッシュ
	/*UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> Mesh;*/

	// 1人称の腕に持たせるStatic メッシュ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMesh> Mesh;

	//腕のメッシュのどのソケットに持たせるか
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FName AttachSocketName = TEXT("GripPoint");

	//撃つたびに腕で再生するアニメーション
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UAnimMontage> FireMontage;

	//リロード中に腕で再生するアニメーション
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UAnimMontage> ReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> ReloadSound;

	//予備弾が無限か(サブ武器)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	bool HasInfiniteReserveAmmo() const { return Slot == ESavaWeaponSlot::Secondary; }
};
