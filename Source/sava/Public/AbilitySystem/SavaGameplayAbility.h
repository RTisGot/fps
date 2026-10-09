// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SavaGameplayAbility.generated.h"

class AsavaCharacter;
class USavaCharacterMovementComponent;

//いつ発動するか
UENUM(BlueprintType)
enum class ESavaAbilityActivationPolicy : uint8
{
	//ボタンを押したら発動(終了は能力側で EndAbility を呼ぶ)
	OnInputTriggered,
	//押している間だけ発動し、離したら自動で終了
	WhileInputActive,
	//付与された時点で自動発動(常時効果・パッシブ用)
	OnSpawn,
};

//スキル・ガジェット・武器の能力はすべてこのクラスを親にする
//・通信: 押した瞬間に自分の画面で発動し(予測)、サーバーが確認する
//・死亡中・ラウンド開始前は発動できない(Activation Blocked Tags に State.Dead / State.RoundFrozen が入っている)
//・Ability.Type.Skill を持つ能力は、旗を運んでいる間(State.CarryingFlag)は発動できない
//・クールダウンは Cooldown Duration と Cooldown Tags を入れるだけで動く
//・個数制(ガジェット)は Max Charges を入れるだけで動く
UCLASS(Abstract)
class SAVA_API USavaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USavaGameplayAbility();

	ESavaAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	//この能力を使っているキャラクター
	UFUNCTION(BlueprintPure, Category = "Sava|Ability")
	AsavaCharacter* GetSavaCharacterFromActorInfo() const;

	//キャラクターの移動コンポーネント(キャラクターを動かす能力用)
	UFUNCTION(BlueprintPure, Category = "Sava|Ability")
	USavaCharacterMovementComponent* GetSavaMovementFromActorInfo() const;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual UGameplayEffect* GetCostGameplayEffect() const override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Ability")
	ESavaAbilityActivationPolicy ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;

	//クールダウン秒数(0 ならクールダウンなし)。Commit Ability を呼んだ時点で開始する
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Cooldown")
	FScalableFloat CooldownDuration;

	//クールダウン中に付くタグ(例: Cooldown.Skill.<能力名>)。能力ごとに別のタグにする
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Cooldown", meta = (Categories = "Cooldown"))
	FGameplayTagContainer CooldownTags;

	//ガジェットの個数(0 なら個数制なし)。1 以上にすると、Commit Ability のたびに GadgetCharges が 1 減り、0 なら発動できない
	//この能力が付与されたとき(リスポーン時など)に満タンになる。1 人が持つ個数制の能力は 1 つだけにする
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Charges", meta = (ClampMin = "0"))
	int32 MaxCharges = 0;

private:
	//GetCooldownTags の戻り値用(親クラスのタグ + CooldownTags)
	UPROPERTY(Transient)
	FGameplayTagContainer TempCooldownTags;
};
