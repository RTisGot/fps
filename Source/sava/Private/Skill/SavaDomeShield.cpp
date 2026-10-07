#include "Skill/SavaDomeShield.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ASavaDomeShield::ASavaDomeShield()
{
	PrimaryActorTick.bCanEverTick = false;

	//========================================
	// Network
	//========================================

	bReplicates = true;

	// 遠くから撃つ人の画面にもシールドが無いと、その人の弾が素通りしてしまうので
	// 距離に関係なく全員へ同期する(数が少ないので負荷は問題にならない)
	bAlwaysRelevant = true;

	//========================================
	// 見た目
	//========================================

	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	RootComponent = DomeMesh;

	// 弾は計算で止めるので、当たり判定は持たない(人もグレネードも通り抜ける)
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DomeMesh->SetCanEverAffectNavigation(false);
	DomeMesh->CastShadow = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		DomeMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void ASavaDomeShield::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 見た目の大きさを Radius に合わせる(Spawn 時に全員の画面で呼ばれる)
	DomeMesh->SetRelativeScale3D(FVector(Radius / MeshBaseRadius));
}

void ASavaDomeShield::BeginPlay()
{
	Super::BeginPlay();

	// 消すのはサーバーだけ(消えたことは全員へ同期される)
	if (HasAuthority() && Duration > 0.0f)
	{
		SetLifeSpan(Duration);
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 24, FColor::Cyan, false,
			Duration > 0.0f ? Duration : 10.0f);
	}

	OnShieldDeployed();
}

void ASavaDomeShield::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		OnShieldExpired();
	}

	Super::EndPlay(EndPlayReason);
}

bool ASavaDomeShield::IntersectDome(
	const FVector& Center,
	float DomeRadius,
	const FVector& Start,
	const FVector& End,
	float& OutTime)
{
	// 線分上の点 P(t) = Start + Dir * t と球の表面の交点を求める
	// |P(t) - Center|^2 = R^2 を t の 2 次方程式として解く
	const FVector Dir = End - Start;
	const FVector FromCenter = Start - Center;

	const float A = Dir.SizeSquared();
	if (A <= KINDA_SMALL_NUMBER || DomeRadius <= 0.0f)
	{
		return false;
	}

	const float B = 2.0f * FVector::DotProduct(FromCenter, Dir);
	const float C = FromCenter.SizeSquared() - DomeRadius * DomeRadius;
	const float Discriminant = B * B - 4.0f * A * C;
	if (Discriminant < 0.0f)
	{
		// 球にかすりもしない
		return false;
	}

	const float Sqrt = FMath::Sqrt(Discriminant);
	const float Times[2] = { (-B - Sqrt) / (2.0f * A), (-B + Sqrt) / (2.0f * A) };

	// 手前の交点から順に、線分の範囲内で上半分(ドーム側)にあるものを探す
	// (外から撃つと手前の交点、中から撃つと奥の交点で止まる)
	for (const float Time : Times)
	{
		if (Time < 0.0f || Time > 1.0f)
		{
			continue;
		}

		if ((Start + Dir * Time).Z >= Center.Z)
		{
			OutTime = Time;
			return true;
		}
	}

	return false;
}

ASavaDomeShield* ASavaDomeShield::FindBlockingShield(
	UWorld* World,
	const FVector& Start,
	const FVector& End,
	FVector& OutLocation,
	FVector& OutNormal)
{
	if (!World)
	{
		return nullptr;
	}

	ASavaDomeShield* ClosestShield = nullptr;
	float ClosestTime = TNumericLimits<float>::Max();

	// シールドは同時に数個しかないので、毎回すべて調べても軽い
	for (TActorIterator<ASavaDomeShield> It(World); It; ++It)
	{
		ASavaDomeShield* Shield = *It;
		if (!IsValid(Shield))
		{
			continue;
		}

		float Time = 0.0f;
		if (IntersectDome(Shield->GetActorLocation(), Shield->Radius, Start, End, Time) && Time < ClosestTime)
		{
			ClosestTime = Time;
			ClosestShield = Shield;
		}
	}

	if (!ClosestShield)
	{
		return nullptr;
	}

	const FVector Dir = End - Start;
	OutLocation = Start + Dir * ClosestTime;

	// 面の向きは撃った側を向ける(中から撃ったときは内向き)
	OutNormal = (OutLocation - ClosestShield->GetActorLocation()).GetSafeNormal();
	if (FVector::DotProduct(OutNormal, Dir) > 0.0f)
	{
		OutNormal = -OutNormal;
	}

	return ClosestShield;
}
