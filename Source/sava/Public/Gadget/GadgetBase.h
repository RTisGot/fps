// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GadgetData.h"
#include "GadgetBase.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UENUM(BlueprintType)
enum class EGadgetState : uint8
{
	Unused,
	Thrown,
	Flying,
	Landed,
	Detonating,
	Destroyed
};

UCLASS()
class SAVA_API AGadgetBase : public AActor
{
	GENERATED_BODY()

public:

	AGadgetBase();

protected:

	virtual void BeginPlay() override;

public:

	//========================================
	// 投擲
	//========================================

	/**
	 * ガジェットを投擲する。
	 *
	 * 実際の入力処理は GameplayAbility 側が担当する。
	 * ガジェット自身は投擲された後の処理を担当する。
	 *
	 * サーバーでのみ実行する。
	 */
	void ThrowGadget(const FVector& Direction);

	//========================================
	// 状態
	//========================================

	EGadgetState GetGadgetState() const
	{
		return m_State;
	}

	bool IsThrown() const
	{
		return m_State == EGadgetState::Thrown
			|| m_State == EGadgetState::Flying;
	}

	AActor* GetGadgetOwner() const
	{
		return m_GadgetOwner;
	}

	//========================================
	// データ
	//========================================

	const FGadgetData& GetGadgetData() const
	{
		return m_GadgetData;
	}

	//========================================
	// 起爆
	//========================================

	/**
	 * ガジェットを起動する。
	 *
	 * 実際の効果処理は派生クラス側で実装する。
	 *
	 * サーバーでのみ実行する。
	 */
	void Detonate();

protected:

	//========================================
	// 派生クラス用
	//========================================

	/**
	 * ガジェットが着地したときに呼ばれる。
	 *
	 * Frag:
	 *     着地時に爆発
	 *
	 * Stun:
	 *     着地後に待機して爆発
	 *
	 * Smoke:
	 *     着地後に効果開始
	 *
	 * などを派生クラス側で決定する。
	 */
	virtual void OnGadgetLanded(const FHitResult& Hit);

	/**
	 * 爆発・効果発生処理。
	 *
	 * AGadgetBaseでは何もしない。
	 * 派生クラス側で実装する。
	 */
	virtual void ApplyGadgetEffect();

	/**
	 * ガジェット終了時の処理。
	 *
	 * VFX/SFXなどを後からここに追加できる。
	 */
	virtual void OnGadgetDestroyed();

	/**
	 * ProjectileMovementが停止したとき。
	 */
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

	//========================================
	// Components
	//========================================

	UPROPERTY(VisibleAnywhere, Category = "Gadget")
	TObjectPtr<USphereComponent> m_CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Gadget")
	TObjectPtr<UProjectileMovementComponent> m_ProjectileMovementComponent;

	//========================================
	// Data
	//========================================

	/**
	 * ガジェット固有の性能値。
	 *
	 * 所持数・クールダウンはここでは管理しない。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Gadget|Data")
	FGadgetData m_GadgetData;

	//========================================
	// Runtime
	//========================================

	/**
	 * このガジェットを使用したプレイヤー。
	 *
	 * 実際のOwner設定はGameplayAbility側で行う。
	 */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> m_GadgetOwner;

	UPROPERTY(Replicated)
	EGadgetState m_State = EGadgetState::Unused;

	//========================================
	// Internal
	//========================================

	void SetGadgetState(EGadgetState NewState);

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};