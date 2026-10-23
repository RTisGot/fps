// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SavaWeaponAbility.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaWeaponReloadAbility.generated.h"

class UAnimMontage;

//リロード(自分の画面とサーバーがそれぞれ時間を数え、両方で弾数を更新する)
//・マガジン式: Reload Time(空なら Empty Reload Time)の Reload Insert Ratio の時点で、弾が全部入る
//・1 発ずつ込める: Reload Start Time の後、Reload Round Time ごとに 1 発入る。込め終わったら Reload End Time 待つ
//・弾が入る前は撃てない。入った後は、撃つ・ダッシュで残りの動作を飛ばせる(1 発ずつ込める武器は、1 発でも入っていれば撃てる)
//・弾が入る前に撃つボタンを押し直しておくと、弾が入った瞬間に撃つ
//・持ち替え・死亡で中断すると、それまでに入った弾だけ残る
//・サーバーは弾を込め終えたら終わる(残りの動作は見た目だけなので待たない)
//・マガジンが満タン、または予備弾がないときは発動しない
UCLASS()
class SAVA_API USavaWeaponReloadAbility : public USavaWeaponAbility
{
	GENERATED_BODY()

public:
	USavaWeaponReloadAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	//今動いているリロード(無ければ nullptr)
	static USavaWeaponReloadAbility* FindActiveReload(const UAbilitySystemComponent* AbilitySystem);

	//今撃てば、リロードの残りを飛ばして撃てるか(弾が入った後 / 1 発ずつ込める武器で 1 発でも入っている)
	bool CanBeInterruptedByFire() const;

protected:
	//リロードを始めた(自分の画面だけ)。Reload Time はリロードにかかる秒数(1 発ずつ込める武器は、満タンまで込めた場合の見込み)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnReloadStarted(float ReloadTime);

	//弾が入った(自分の画面だけ。1 発ずつ込める武器は 1 発ごと)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnReloadAmmoLoaded(int32 AmmoInMagazine);

	//リロードが終わった(自分の画面だけ)。bCompleted = 弾が入ったか / false なら弾が入る前に中断した
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnReloadEnded(bool bCompleted);

	//サーバー: 弾が入る直前に中断されたとき、この時間以内なら弾を入れる(通信の遅れで、サーバーのリロードは少し遅れて進むため)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon|Server", meta = (ClampMin = "0", ForceUnits = "s"))
	float ServerLoadTolerance = 0.2f;

private:
	enum class EPhase : uint8
	{
		None,
		//マガジン式: 弾が入るまで
		BeforeInsert,
		//マガジン式: 弾が入った後の残りの動作
		AfterInsert,
		//1 発ずつ込める: 込め始めるまで
		RoundStart,
		//1 発ずつ込める: 1 発込めている途中
		Round,
		//1 発ずつ込める: 込め終わった後の動作
		RoundEnd,
	};

	//Duration 秒後に OnPhaseElapsed を呼ぶ(0 以下ならすぐ呼ぶ)
	void StartPhase(EPhase NewPhase, float Duration);
	void OnPhaseElapsed();

	//1 発ずつ込める: 次の 1 発を込め始める。もう込められなければ終わりの動作へ
	void StartNextRoundOrEnd();

	//弾を入れる。入れた弾数を返す
	int32 LoadRounds(int32 MaxRounds);

	//弾が入る前から押していた撃つボタンで撃つ。撃った(このリロードが終わった)なら true
	bool TryFireBuffered();

	//自分の画面: 毎フレーム、撃つボタンを離したか・ダッシュしたかを調べる
	void OnLocalTick();

	bool IsFireInputPressed() const;
	bool IsServerOnly() const;

	//最後まで終えた
	void Finish();

	void PlayReloadMontage(UAnimMontage* Montage, float Duration);
	void StopReloadMontage();

	//リロードしている枠
	ESavaWeaponSlot ReloadSlot = ESavaWeaponSlot::Primary;

	EPhase Phase = EPhase::None;
	double PhaseStartTime = 0.0;
	float PhaseDuration = 0.0f;
	FTimerHandle PhaseTimerHandle;
	FTimerHandle LocalTickTimerHandle;

	//マガジンが空の状態から始めた
	bool bEmptyReload = false;
	//このリロードで入れられる弾数の上限(薬室の 1 発を含む)
	int32 Capacity = 0;
	//弾が 1 発でも入った
	bool bAmmoLoaded = false;
	//最後まで終えた
	bool bFinished = false;
	//始めたときから撃つボタンを押したまま(一度離すまでは、弾が入っても撃たない。弾切れの自動リロードで撃ち続けないため)
	bool bFireHeldSinceStart = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage;
};
