#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "SavaGrappleAbility.generated.h"

class ASavaGrappleRope;
struct FGameplayAbilityTargetDataHandle;

//押した瞬間に狙った固定の面をつかみ、押している間そこへ引っ張る
//
//流れ:
//  押す       → 自分の画面でアンカーを探す(なければ失敗。クールダウンは消費しない)
//             → クールダウン開始 → 自分の画面で引っ張り開始・アンカーをサーバーへ送る
//  サーバー   → 届いたアンカーを確認(距離・見通し・固定の面か) → サーバーでも同じ引っ張りを開始
//  離す・到着 → 終了(サーバーにも伝わる)
//
//引っ張りは RootMotionSource なので、自分の画面とサーバーの両方で同じものを付ける必要がある
//
//※ Blueprint で子クラスを作る場合、Event ActivateAbility は使わないこと(On Grapple Started / Finished だけを実装する)
UCLASS()
class SAVA_API USavaGrappleAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	USavaGrappleAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	//つかめる最大距離
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "100", Units = "cm"))
	float MaxRange = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "100", Units = "cm/s"))
	float PullSpeed = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0", Units = "cm/s"))
	float SteeringSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0", Units = "cm"))
	float MaxLateralOffset = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0.1"))
	float SteeringResponse = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0", Units = "cm/s"))
	float JumpLiftSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "0.1", Units = "s"))
	float MaxPullDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "1", Units = "cm"))
	float SurfaceClearance = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	//サーバーの確認で許す距離のずれ(通信の遅れで、サーバーから見た位置が少し違うため)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Server", meta = (ClampMin = "0", Units = "cm"))
	float ServerDistanceTolerance = 200.0f;

	/** Replace with a Blueprint child to customize the rope mesh/material. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple")
	TSubclassOf<ASavaGrappleRope> RopeClass;

	//引っ張り開始(自分の画面とサーバーの両方で呼ばれる)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Grapple")
	void OnGrappleStarted(FVector AnchorLocation);

	//引っ張り終了(自分の画面とサーバーの両方で呼ばれる)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Grapple")
	void OnGrappleFinished(bool bWasCancelled);

private:
	bool GetAim(FVector& Location, FVector& Direction) const;
	bool FindAnchor(FHitResult& Hit) const;
	bool IsAnchorAllowed(const FHitResult& Hit) const;

	//クライアントから届いたアンカーが、サーバーから見てもつかめるか
	bool IsAnchorPlausible(const FHitResult& ClientHit) const;

	//アンカー(面の位置と向き)から、引っ張った先のカプセルの位置を求める。引っ張れなければ false
	bool ComputePullTarget(const FVector& AnchorLocation, const FVector& AnchorNormal, FVector& OutTargetLocation) const;

	void OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag);
	void StartPull(const FHitResult& Anchor);
	void TickPull(float DeltaTime);
	void Finish(bool bCancelled);

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnDied();

	UPROPERTY(Transient)
	TObjectPtr<ASavaGrappleRope> Rope;

	FVector TargetLocation = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	float Elapsed = 0.0f;
	float StalledTime = 0.0f;
	bool bPulling = false;
	bool bEnding = false;
	bool bListeningForServerTargetData = false;
	uint16 PullSourceID = 0;
};
