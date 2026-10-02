// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SavaTestDummyCharacter.h"
#include "SavaCombatTestDummyCharacter.generated.h"

class UTextRenderComponent;
struct FOnAttributeChangeData;
struct FHitResult;

//武器性能テスト用ダミー
//・被弾情報(部位/ダメージ)の表示
//・TTK 計測
//・死亡後の自動リセット
//・部位ごとの当たり判定可視化
UCLASS()
class SAVA_API ASavaCombatTestDummyCharacter : public ASavaTestDummyCharacter
{
    GENERATED_BODY()

public:
    ASavaCombatTestDummyCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    //武器側から「どこに・いくら当たったか」を通知する
    void NotifyWeaponImpact(const FHitResult& Hit, float Damage, bool bHeadshot);

protected:
    //頭上に出すステータステキスト
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
    TObjectPtr<UTextRenderComponent> StatusTextComponent;

    //部位のデバッグ表示を常時描画するか
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Debug")
    bool bVisualizeHitRegions = true;

    //部位デバッグ描画の表示時間(0 なら毎フレーム描画で常時表示相当)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Debug", meta = (ClampMin = "0.1", ForceUnits = "s"))
    float RegionDrawDuration = 0.0f;

    //部位デバッグ描画の線の太さ
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Debug", meta = (ClampMin = "0.1"))
    float RegionLineThickness = 1.5f;

    //死亡後リセットまでの最小時間
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Reset", meta = (ClampMin = "0.1", ForceUnits = "s"))
    float ResetDelayMinSeconds = 1.0f;

    //死亡後リセットまでの最大時間
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Reset", meta = (ClampMin = "0.1", ForceUnits = "s"))
    float ResetDelayMaxSeconds = 2.0f;

    //死亡時に自動でリセットするか
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Reset")
    bool bAutoResetWhenDead = true;

    //着弾点(球/テキスト)の表示時間
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy|Impact", meta = (ClampMin = "0.1", ForceUnits = "s"))
    float ImpactDisplaySeconds = 2.5f;

private:
    //ダミー内で使う部位分類
    enum class EHitRegion : uint8
    {
        Unknown,
        Head,
        Torso,
        Arm,
        Leg,
    };

    //HP 変更を監視して TTK を計測
    void HandleHealthChanged(const FOnAttributeChangeData& Data);
    //HP と統計を初期状態に戻す
    void ResetDummy();
    //被弾統計をクリアする
    void ResetSessionStats();
    //頭上テキストを更新
    void UpdateStatusText();
    //ランダム遅延でリセット予約
    void ScheduleReset();
    //部位ごとのデバッグ形状を描画
    void DrawHitRegionDebug() const;
    //部位ラベル文字を描画
    void DrawRegionLabel(const FVector& WorldLocation, EHitRegion HitRegion) const;
    //Hit 情報から部位を推定
    EHitRegion ClassifyHitRegion(const FHitResult& Hit, bool bHeadshot) const;
    const TCHAR* HitRegionToLabel(EHitRegion HitRegion) const;
    FColor GetRegionColor(EHitRegion HitRegion) const;
    float GetRegionMarkerSize(EHitRegion HitRegion) const;

    //死亡後リセット用タイマー
    FTimerHandle ResetTimerHandle;
    //TTK 計測開始時刻
    double FirstDamageTime = 0.0;
    //UI の前回更新時刻
    double LastUiRefreshTime = 0.0;
    //直近の被ダメージ
    float LastDamage = 0.0f;
    //合計被ダメージ
    float TotalDamage = 0.0f;
    //最後に計測した TTK
    float LastTTK = 0.0f;
    int32 HeadHitCount = 0;
    int32 TorsoHitCount = 0;
    int32 ArmHitCount = 0;
    int32 LegHitCount = 0;
    int32 UnknownHitCount = 0;
    //TTK 計測中フラグ
    bool bTTKRunning = false;
    //直近で判定した部位
    EHitRegion LastHitRegion = EHitRegion::Unknown;
};
