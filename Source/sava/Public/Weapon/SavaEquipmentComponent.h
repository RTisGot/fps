// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaEquipmentComponent.generated.h"

class USavaWeaponData;
class USkeletalMeshComponent;

//1 つの枠の装備(サーバーが決めて全員へ同期する)
USTRUCT(BlueprintType)
struct FSavaWeaponLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TObjectPtr<USavaWeaponData> Weapon;

	//この武器に付けたカスタム
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FSavaWeaponModifier> Modifiers;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSavaOnWeaponChanged, ESavaWeaponSlot, Slot, USavaWeaponData*, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSavaOnAmmoChanged, ESavaWeaponSlot, Slot, int32, AmmoInMagazine, int32, ReserveAmmo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnWeaponMuzzleFlash, FVector, MuzzleLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSavaOnWeaponTracer, FVector, MuzzleLocation, TArray<FVector>, TraceEnds);

//キャラクターが持つ武器(メイン・サブの 2 枠)を管理する
//・装備(どの武器か・カスタム)はサーバーが決めて全員へ同期する
//・弾数は自分の画面とサーバーがそれぞれ数える(撃つたびに通信しないため)。ずれやすい予備弾はリロード時にサーバーの値へ合わせる
//・ADS の視野角・反動・拡散は自分の画面だけで計算する
UCLASS(ClassGroup = (Sava), meta = (BlueprintSpawnableComponent))
class SAVA_API USavaEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USavaEquipmentComponent();

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static USavaEquipmentComponent* FindEquipmentComponent(const AActor* Actor);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//--------------------------------装備(サーバーのみ)

	//枠に武器を装備する(ロードアウト画面から呼ぶ想定)。弾は満タンになる
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Weapon")
	void SetLoadout(ESavaWeaponSlot Slot, USavaWeaponData* Weapon, const TArray<FSavaWeaponModifier>& Modifiers);

	//全部の武器の弾を満タンにする(同じキャラクターのまま復活させる場合に呼ぶ)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Weapon")
	void RefillAllAmmo();

	//--------------------------------状態(UI などから読む)

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	ESavaWeaponSlot GetCurrentSlot() const { return CurrentSlot; }

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	USavaWeaponData* GetWeaponData(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	USavaWeaponData* GetCurrentWeaponData() const { return GetWeaponData(CurrentSlot); }

	//カスタムを適用した数値
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	FSavaWeaponStats GetStats(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	int32 GetAmmoInMagazine(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	int32 GetReserveAmmo(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	bool HasInfiniteReserveAmmo(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	bool CanReload(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	bool CanSwapWeapon() const;

	//今の拡散の角度(クロスヘアの広がりに使う。自分の画面だけ正しい値になる)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	float GetCurrentSpreadAngle() const;

	//ADS の進み具合(0 = 腰撃ち、1 = ADS に入りきった。自分の画面だけ)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	float GetADSAlpha() const { return ADSAlpha; }

	//装備中の武器が変わった(持ち替え・装備の変更)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Weapon")
	FSavaOnWeaponChanged OnWeaponChanged;

	//弾数が変わった(自分の画面では撃った瞬間に呼ばれる)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Weapon")
	FSavaOnAmmoChanged OnAmmoChanged;

	//--------------------------------武器の能力から呼ぶ

	//持ち替える(自分の画面とサーバーの両方で呼ぶ)
	void SwapWeapon();

	//自分の画面: 次の 1 発まで残り何秒か
	float GetRemainingFireInterval(ESavaWeaponSlot Slot) const;

	//自分の画面: 弾を 1 発使う。マガジンが空なら false
	bool TryConsumeLocalShot(ESavaWeaponSlot Slot);

	//サーバー: クライアントから届いた 1 発分の弾を使う。マガジンが空、または撃つ間隔が短すぎれば false
	//(短すぎる場合も弾は使う。自分の画面の弾数とずれないようにするため)
	bool TryConsumeServerShot(ESavaWeaponSlot Slot, float IntervalTolerance);

	//リロードを完了する(自分の画面とサーバーの両方で呼ぶ)
	void FinishReload(ESavaWeaponSlot Slot);

	//自分の画面: 反動で視点を動かす
	void ApplyRecoil(ESavaWeaponSlot Slot);

	//自分の画面: 腕のメッシュ(アニメーションの再生用)
	USkeletalMeshComponent* GetFirstPersonMesh() const;

	//--------------------------------武器演出

	//現在装備している武器の銃口位置を取得する。
	bool GetCurrentMuzzleLocation(FVector& OutLocation) const;

	//武器の発砲演出をサーバーへ通知する。
	void NotifyWeaponFireVisual(const FVector& MuzzleLocation, const TArray<FVector>& TraceEnds);

protected:
	//装備を用意するまでの仮の武器(ロードアウト画面ができるまで、キャラクターの Blueprint で設定する)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Weapon")
	TObjectPtr<USavaWeaponData> DefaultPrimaryWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Sava|Weapon")
	TObjectPtr<USavaWeaponData> DefaultSecondaryWeapon;

	//発砲時にマズルフラッシュを通知する
	UPROPERTY(BlueprintAssignable, Category = "Sava|Weapon|Visual")
	FSavaOnWeaponMuzzleFlash OnWeaponMuzzleFlashEvent;

	//発砲時にトレーサーを通知する
	UPROPERTY(BlueprintAssignable, Category = "Sava|Weapon|Visual")
	FSavaOnWeaponTracer OnWeaponTracerEvent;

private:
	//1 つの枠の、自分のマシン上での状態(同期しない)
	struct FSlotState
	{
		//Stats を計算したときの装備(装備が変わったか調べる用)
		const USavaWeaponData* AppliedWeapon = nullptr;
		TArray<FSavaWeaponModifier> AppliedModifiers;

		FSavaWeaponStats Stats;
		int32 AmmoInMagazine = 0;
		int32 ReserveAmmo = 0;
		double LocalLastFireTime = -1.0e9;
		double ServerLastFireTime = -1.0e9;
	};

	UFUNCTION()
	void OnRep_Loadout();

	UFUNCTION()
	void OnRep_CurrentSlot();

	//サーバーから、予備弾の数を合わせる
	UFUNCTION(Client, Reliable)
	void ClientSyncReserveAmmo(ESavaWeaponSlot Slot, int32 ReserveAmmo);

	UFUNCTION(Client, Reliable)
	void ClientRefillAllAmmo();

	//装備に合わせて数値を計算し直す。武器が変わった枠は弾を満タンにする
	void ApplyLoadout();
	void RefillAmmoLocal();

	void UpdateWeaponMesh();
	void UpdateADS(float DeltaTime);
	void UpdateRecoilRecovery(float DeltaTime);

	//プレイヤーが自分で視点を下げた分は、反動で戻す量から引く(戻しすぎないように)
	void AbsorbPlayerPitchInput(float CurrentPitch);

	FSlotState* FindSlotState(ESavaWeaponSlot Slot);
	const FSlotState* FindSlotState(ESavaWeaponSlot Slot) const;
	const FSavaWeaponLoadoutEntry* FindLoadoutEntry(ESavaWeaponSlot Slot) const;
	APlayerController* GetLocalPlayerController() const;
	void BroadcastAmmoChanged(ESavaWeaponSlot Slot);

	//メイン・サブの装備(添え字 = ESavaWeaponSlot)
	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	TArray<FSavaWeaponLoadoutEntry> Loadout;

	//持っている枠。自分の画面では持ち替えの瞬間に変わるので、他のプレイヤーにだけ送る
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSlot)
	ESavaWeaponSlot CurrentSlot = ESavaWeaponSlot::Primary;

	FSlotState SlotStates[2];

	//1 人称の腕に持たせる武器のメッシュ
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComponent;

	//クライアントから受け取った発砲演出を全員へ通知する
	UFUNCTION(Server, Unreliable)
	void ServerNotifyWeaponFireVisual(
		FVector_NetQuantize MuzzleLocation,
		const TArray<FVector_NetQuantize>& TraceEnds);

	//全クライアントへ発砲演出を通知する
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastWeaponFireVisual(
		FVector_NetQuantize MuzzleLocation,
		const TArray<FVector_NetQuantize>& TraceEnds);

	//ADS
	float ADSAlpha = 0.0f;
	float BaseFieldOfView = 0.0f;

	//反動: まだ戻していない跳ね上がりの量
	float RecoilPitchToRecover = 0.0f;
	float LastRecoilPitch = 0.0f;
	double LastRecoilTime = -1.0e9;
	float RecoilRecoveryDelay = 0.0f;
	float RecoilRecoverySpeed = 0.0f;
};
