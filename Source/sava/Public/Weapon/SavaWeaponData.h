// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaWeaponData.generated.h"

class UAnimMontage;
class UNiagaraSystem;
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
	USavaWeaponData();

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

	//--------------------------------カスタマイズ(ロードアウト画面にはここに並べた順で出る)

	//スライダー(何かを強くする代わりに何かが弱くなる。新しく作った武器には、おすすめの 6 本が入っている)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning", meta = (TitleProperty = "Id"))
	TArray<FSavaWeaponTuningSlider> TuningSliders;

	//切り替え式(サイトなど。選択肢から 1 つ選ぶ)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning", meta = (TitleProperty = "Id"))
	TArray<FSavaWeaponTuningToggle> TuningToggles;

	//この武器に無い Id を消し、スライダーの値を範囲内に収め、選べない選択肢を最初の選択肢にしたものを返す
	//(並びはこの武器のスライダー・切り替えの順にそろう)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	FSavaWeaponTuning SanitizeTuning(const FSavaWeaponTuning& Tuning) const;

	//カスタマイズを、この武器のカスタムにする(スライダー 1 本・切り替え 1 つごとに 1 つ)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	TArray<FSavaWeaponModifier> MakeTuningModifiers(const FSavaWeaponTuning& Tuning) const;

	//カスタマイズを適用した数値(ロードアウト画面で数値を見せる用)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	FSavaWeaponStats GetTunedStats(const FSavaWeaponTuning& Tuning) const;

	//Id でスライダー・切り替えを探す(無ければ nullptr。重なっていれば最初のもの)
	const FSavaWeaponTuningSlider* FindTuningSlider(FName Id) const;
	const FSavaWeaponTuningToggle* FindTuningToggle(FName Id) const;

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

	//武器アタッチ位置からの銃口(マズル)の相対位置。モーションに依存しない固定位置になる
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (ForceUnits = "cm"))
	FVector MuzzleOffset = FVector::ZeroVector;

	//武器メッシュの銃口(マズル)ソケット名。空の場合は MuzzleOffset を使う
	//このソケットはアニメーションで動かないボーンに定義すること
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FName MuzzleSocketName = NAME_None;

	//撃つたびに銃口に出すエフェクト。空なら、キャラクターの Equipment Component の Muzzle Flash System を使う
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	//マズル位置のデバッグ描画の色
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FColor MuzzleFlashColor = FColor::Red;

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
