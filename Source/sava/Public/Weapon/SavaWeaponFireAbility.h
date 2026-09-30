// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SavaWeaponAbility.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaWeaponFireAbility.generated.h"

struct FGameplayAbilityTargetDataHandle;

//ヒットスキャン武器の「撃つ」能力。数値は今持っている武器(SavaWeaponData)から読む
//
//流れ(1 回撃つごと):
//  自分の画面 → 弾を 1 発使う → 拡散させてレイを飛ばす(Pellet Count 本) → 演出・反動 → 当たった結果をサーバーへ送る
//  サーバー   → 弾を 1 発使う → 結果を確認する → 問題なければ距離・部位に応じたダメージ → On Hit Confirmed
//
//撃ち方: 単発 = 押すたびに 1 回 / バースト = 押すたびに Burst Count 回 / フルオート = 押している間
//マガジンが空で撃とうとすると、自動でリロードする
//
//※ Blueprint で子クラスを作る場合、Event ActivateAbility は使わないこと(下のイベントだけを実装する)
UCLASS()
class SAVA_API USavaWeaponFireAbility : public USavaWeaponAbility
{
	GENERATED_BODY()

public:
	USavaWeaponFireAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	//--------------------------------Blueprint で実装するイベント

	//撃った(自分の画面だけ)。マズルフラッシュ・弾道など。Hits は弾ごとの結果(外れた弾は bBlockingHit が false で、ImpactPoint は射程の端)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnFired(const TArray<FHitResult>& Hits);

	//サーバーが当たりを認めてダメージを与えた(サーバーだけ。弾ごとに呼ばれる)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnHitConfirmed(const FHitResult& Hit, float AppliedDamage, bool bHeadshot);

	//--------------------------------設定

	//レイ判定に使うチャンネル(Project Settings → Collision の Weapon)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel2;

	//レイを飛ばす最大距離(射程の上限はないので、マップより十分長くする)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxTraceDistance = 50000.0f;

	//カプセルの上端からこの高さまでに当たったら頭とみなす(キャラクターのメッシュができたら骨での判定に切り替える)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HeadshotHeight = 30.0f;

	//--------------------------------サーバーの確認(クライアントから届いた結果をどこまで許すか)

	//撃った位置と、サーバーから見たプレイヤーの視点とのずれ(通信の遅れで少しずれるため)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon|Server", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ServerViewTolerance = 150.0f;

	//当たった位置と、サーバーから見た相手の位置とのずれ(巻き戻しをしないので、相手の移動分を許す)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon|Server", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ServerHitTolerance = 250.0f;

	//頭に当たったという結果を認める、サーバーから見た頭の位置とのずれ(胴体より厳しくする)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon|Server", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ServerHeadshotTolerance = 120.0f;

	//撃つ間隔がこの割合より短ければダメージを与えない(0.5 = 本来の間隔の半分まで許す。通信で届く間隔がばらつくため)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Weapon|Server", meta = (ClampMin = "0", ClampMax = "1"))
	float ServerFireIntervalTolerance = 0.5f;

private:
	//自分の画面で 1 回撃つ
	void FireShot();

	//視点から、拡散を付けて弾の数だけレイを飛ばす
	void TraceShot(const FSavaWeaponStats& Stats, float SpreadAngle, TArray<FHitResult>& OutHits) const;

	//頭に当たったか(自分の画面での判定)
	bool IsHeadHit(const FHitResult& Hit) const;

	//サーバーで 1 回分の結果を処理する。bFromRemoteClient ならクライアントの結果を確認してから使う
	void ProcessShotOnServer(const TArray<FHitResult>& Hits, bool bFromRemoteClient);

	//クライアントから届いた当たりが、サーバーから見てありえるか
	bool IsHitPlausible(const FHitResult& Hit) const;
	bool IsHeadHitPlausible(const FHitResult& Hit) const;

	void OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag);

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	//撃つのをやめてリロードする(自分の画面だけ)
	void EndAndReload();

	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	//発動したときに持っていた枠(撃っている途中で持ち替えたら止める)
	ESavaWeaponSlot FiringSlot = ESavaWeaponSlot::Primary;

	//あと何回撃つか(-1 = 離すまで)
	int32 ShotsRemaining = 0;

	FTimerHandle FireTimerHandle;
	bool bListeningForServerTargetData = false;
};
