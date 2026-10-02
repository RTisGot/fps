// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaCombatTestDummyCharacter.h"

#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

ASavaCombatTestDummyCharacter::ASavaCombatTestDummyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    //頭上表示用テキスト
    StatusTextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
    StatusTextComponent->SetupAttachment(GetRootComponent());
    StatusTextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
    StatusTextComponent->SetHorizontalAlignment(EHTA_Center);
    StatusTextComponent->SetWorldSize(24.0f);
    StatusTextComponent->SetTextRenderColor(FColor::White);
    StatusTextComponent->SetText(FText::FromString(TEXT("Combat Dummy")));
}

void ASavaCombatTestDummyCharacter::BeginPlay()
{
    Super::BeginPlay();

    //Health の変化を監視して TTK 開始/終了を判定する
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(USavaAttributeSet::GetHealthAttribute())
            .AddUObject(this, &ASavaCombatTestDummyCharacter::HandleHealthChanged);
    }

    ResetSessionStats();
    UpdateStatusText();
}

void ASavaCombatTestDummyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    //部位ごとの当たり判定を常時可視化
    if (bVisualizeHitRegions)
    {
        DrawHitRegionDebug();
    }

    if (!bTTKRunning)
    {
        return;
    }

    //TTK 計測中は表示を定期更新
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastUiRefreshTime >= 0.05)
    {
        UpdateStatusText();
    }
}

void ASavaCombatTestDummyCharacter::NotifyWeaponImpact(const FHitResult& Hit, float Damage, bool bHeadshot)
{
    if (Damage <= 0.0f)
    {
        return;
    }

    //被弾ログ(直近/合計/部位別カウント)を更新
    LastDamage = Damage;
    TotalDamage += Damage;
    LastHitRegion = ClassifyHitRegion(Hit, bHeadshot);

    switch (LastHitRegion)
    {
    case EHitRegion::Head:
        ++HeadHitCount;
        break;
    case EHitRegion::Torso:
        ++TorsoHitCount;
        break;
    case EHitRegion::Arm:
        ++ArmHitCount;
        break;
    case EHitRegion::Leg:
        ++LegHitCount;
        break;
    default:
        ++UnknownHitCount;
        break;
    }

    if (UWorld* World = GetWorld())
    {
        //着弾点を一定時間だけ表示
        const float DrawTime = FMath::Max(0.1f, ImpactDisplaySeconds);
        const FColor Color = GetRegionColor(LastHitRegion);
        const float MarkerSize = GetRegionMarkerSize(LastHitRegion);

        DrawDebugSphere(World, Hit.ImpactPoint, MarkerSize, 10, Color, false, DrawTime);
        DrawDebugPoint(World, Hit.ImpactPoint, MarkerSize * 2.0f, Color, false, DrawTime);
        DrawDebugString(World, Hit.ImpactPoint + FVector(0.0f, 0.0f, MarkerSize + 12.0f),
            FString::Printf(TEXT("%s %.1f"), HitRegionToLabel(LastHitRegion), Damage), nullptr, Color, DrawTime);
    }

    UpdateStatusText();
}

void ASavaCombatTestDummyCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    const float ActualDamage = FMath::Max(Data.OldValue - Data.NewValue, 0.0f);
    if (ActualDamage > KINDA_SMALL_NUMBER && !bTTKRunning)
    {
        //最初の被弾で TTK 計測開始
        bTTKRunning = true;
        FirstDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }

    if (Data.NewValue <= 0.0f && bTTKRunning)
    {
        //死亡時に TTK を確定してリセット予約
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        LastTTK = static_cast<float>(Now - FirstDamageTime);
        bTTKRunning = false;
        ScheduleReset();
    }

    UpdateStatusText();
}

void ASavaCombatTestDummyCharacter::ResetDummy()
{
    if (!AttributeSet)
    {
        return;
    }

    //HP とメタ属性を初期化
    AttributeSet->SetDamage(0.0f);
    AttributeSet->SetHealing(0.0f);
    AttributeSet->SetHealth(AttributeSet->GetMaxHealth());

    ResetSessionStats();
    UpdateStatusText();
}

void ASavaCombatTestDummyCharacter::ResetSessionStats()
{
    LastDamage = 0.0f;
    TotalDamage = 0.0f;
    LastTTK = 0.0f;
    HeadHitCount = 0;
    TorsoHitCount = 0;
    ArmHitCount = 0;
    LegHitCount = 0;
    UnknownHitCount = 0;
    bTTKRunning = false;
    LastHitRegion = EHitRegion::Unknown;
    FirstDamageTime = 0.0;
}

void ASavaCombatTestDummyCharacter::UpdateStatusText()
{
    if (!StatusTextComponent)
    {
        return;
    }

    const float CurrentHealth = GetCurrentHealth();
    const float MaxHealth = GetMaxHealth();
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float CurrentTTK = bTTKRunning ? static_cast<float>(Now - FirstDamageTime) : LastTTK;
    const FString TTKText = (bTTKRunning || LastTTK > 0.0f) ? FString::Printf(TEXT("%.2fs"), CurrentTTK) : TEXT("--");

    const FString Status = FString::Printf(
        TEXT("HP %.1f / %.1f\nLast %.1f (%s)\nTTK %s\nHead %d / Torso %d / Arm %d / Leg %d / ? %d\nTotal %.1f"),
        CurrentHealth,
        MaxHealth,
        LastDamage,
        HitRegionToLabel(LastHitRegion),
        *TTKText,
        HeadHitCount,
        TorsoHitCount,
        ArmHitCount,
        LegHitCount,
        UnknownHitCount,
        TotalDamage);

    StatusTextComponent->SetText(FText::FromString(Status));
    StatusTextComponent->SetTextRenderColor(CurrentHealth <= 0.0f ? FColor::Red : FColor::Blue);
    LastUiRefreshTime = Now;
}

void ASavaCombatTestDummyCharacter::ScheduleReset()
{
    if (!bAutoResetWhenDead || !GetWorld())
    {
        return;
    }

    //指定レンジ(最小〜最大)でランダム遅延してリセット
    const float MinDelay = FMath::Max(0.1f, ResetDelayMinSeconds);
    const float MaxDelay = FMath::Max(MinDelay, ResetDelayMaxSeconds);
    const float Delay = FMath::FRandRange(MinDelay, MaxDelay);
    GetWorldTimerManager().SetTimer(ResetTimerHandle, this, &ASavaCombatTestDummyCharacter::ResetDummy, Delay, false);
}

void ASavaCombatTestDummyCharacter::DrawHitRegionDebug() const
{
    UWorld* World = GetWorld();
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!World || !Capsule)
    {
        return;
    }

    const FVector ActorLocation = GetActorLocation();
    const FVector Up = GetActorUpVector();
    const FVector Right = GetActorRightVector();
    const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
    const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float Duration = FMath::Max(0.0f, RegionDrawDuration);
    const float Thickness = FMath::Max(0.1f, RegionLineThickness);

    //Head: 球
    const FVector HeadCenter = ActorLocation + Up * (CapsuleHalfHeight * 0.72f);
    const float HeadRadius = CapsuleRadius * 0.75f;
    DrawDebugSphere(World, HeadCenter, HeadRadius, 16, GetRegionColor(EHitRegion::Head), false, Duration, 0, Thickness);
    DrawRegionLabel(HeadCenter + Up * (HeadRadius + 8.0f), EHitRegion::Head);

    //Torso: カプセル
    const FVector TorsoCenter = ActorLocation + Up * (CapsuleHalfHeight * 0.1f);
    const float TorsoHalf = CapsuleHalfHeight * 0.32f;
    const float TorsoRadius = CapsuleRadius * 0.95f;
    DrawDebugCapsule(World, TorsoCenter, TorsoHalf, TorsoRadius, GetActorQuat(), GetRegionColor(EHitRegion::Torso), false, Duration, 0, Thickness);
    DrawRegionLabel(TorsoCenter + Right * (TorsoRadius + 8.0f), EHitRegion::Torso);

    //Arm: 左右カプセル
    const float ArmHalf = CapsuleHalfHeight * 0.2f;
    const float ArmRadius = CapsuleRadius * 0.32f;
    const FVector ArmBase = ActorLocation + Up * (CapsuleHalfHeight * 0.14f);
    const FVector LeftArmCenter = ArmBase - Right * (CapsuleRadius * 1.35f);
    const FVector RightArmCenter = ArmBase + Right * (CapsuleRadius * 1.35f);
    DrawDebugCapsule(World, LeftArmCenter, ArmHalf, ArmRadius, GetActorQuat(), GetRegionColor(EHitRegion::Arm), false, Duration, 0, Thickness);
    DrawDebugCapsule(World, RightArmCenter, ArmHalf, ArmRadius, GetActorQuat(), GetRegionColor(EHitRegion::Arm), false, Duration, 0, Thickness);
    DrawRegionLabel(RightArmCenter + Right * (ArmRadius + 8.0f), EHitRegion::Arm);

    //Leg: 左右カプセル
    const float LegHalf = CapsuleHalfHeight * 0.26f;
    const float LegRadius = CapsuleRadius * 0.4f;
    const FVector LegBase = ActorLocation - Up * (CapsuleHalfHeight * 0.38f);
    const FVector LeftLegCenter = LegBase - Right * (CapsuleRadius * 0.45f);
    const FVector RightLegCenter = LegBase + Right * (CapsuleRadius * 0.45f);
    DrawDebugCapsule(World, LeftLegCenter, LegHalf, LegRadius, GetActorQuat(), GetRegionColor(EHitRegion::Leg), false, Duration, 0, Thickness);
    DrawDebugCapsule(World, RightLegCenter, LegHalf, LegRadius, GetActorQuat(), GetRegionColor(EHitRegion::Leg), false, Duration, 0, Thickness);
    DrawRegionLabel(RightLegCenter + Right * (LegRadius + 8.0f), EHitRegion::Leg);
}

void ASavaCombatTestDummyCharacter::DrawRegionLabel(const FVector& WorldLocation, EHitRegion HitRegion) const
{
    if (UWorld* World = GetWorld())
    {
        DrawDebugString(World, WorldLocation, HitRegionToLabel(HitRegion), nullptr, GetRegionColor(HitRegion), FMath::Max(0.0f, RegionDrawDuration), false);
    }
}

ASavaCombatTestDummyCharacter::EHitRegion ASavaCombatTestDummyCharacter::ClassifyHitRegion(const FHitResult& Hit, bool bHeadshot) const
{
    if (bHeadshot)
    {
        return EHitRegion::Head;
    }

    //まず骨名で判定
    const FString Bone = Hit.BoneName.ToString().ToLower();
    if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck")))
    {
        return EHitRegion::Head;
    }
    if (Bone.Contains(TEXT("spine")) || Bone.Contains(TEXT("chest")) || Bone.Contains(TEXT("pelvis")))
    {
        return EHitRegion::Torso;
    }
    if (Bone.Contains(TEXT("upperarm")) || Bone.Contains(TEXT("lowerarm")) || Bone.Contains(TEXT("hand")) || Bone.Contains(TEXT("clavicle")))
    {
        return EHitRegion::Arm;
    }
    if (Bone.Contains(TEXT("thigh")) || Bone.Contains(TEXT("calf")) || Bone.Contains(TEXT("foot")) || Bone.Contains(TEXT("leg")))
    {
        return EHitRegion::Leg;
    }

    //骨が取れない場合は高さから近似判定
    const FVector LocalImpact = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
    const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 88.0f;
    const float NormalizedHeight = FMath::Clamp((LocalImpact.Z + HalfHeight) / (2.0f * HalfHeight), 0.0f, 1.0f);
    if (NormalizedHeight > 0.8f)
    {
        return EHitRegion::Head;
    }
    if (NormalizedHeight > 0.35f)
    {
        return EHitRegion::Torso;
    }
    return EHitRegion::Leg;
}

const TCHAR* ASavaCombatTestDummyCharacter::HitRegionToLabel(EHitRegion HitRegion) const
{
    switch (HitRegion)
    {
    case EHitRegion::Head:
        return TEXT("Head");
    case EHitRegion::Torso:
        return TEXT("Torso");
    case EHitRegion::Arm:
        return TEXT("Arm");
    case EHitRegion::Leg:
        return TEXT("Leg");
    default:
        return TEXT("Unknown");
    }
}

FColor ASavaCombatTestDummyCharacter::GetRegionColor(EHitRegion HitRegion) const
{
    switch (HitRegion)
    {
    case EHitRegion::Head:
        return FColor::Magenta;
    case EHitRegion::Torso:
        return FColor::Green;
    case EHitRegion::Arm:
        return FColor::Yellow;
    case EHitRegion::Leg:
        return FColor::Cyan;
    default:
        return FColor::Silver;
    }
}

float ASavaCombatTestDummyCharacter::GetRegionMarkerSize(EHitRegion HitRegion) const
{
    switch (HitRegion)
    {
    case EHitRegion::Head:
        return 10.0f;
    case EHitRegion::Torso:
        return 8.0f;
    case EHitRegion::Arm:
        return 6.5f;
    case EHitRegion::Leg:
        return 7.5f;
    default:
        return 6.0f;
    }
}
