#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SavaDomeShield.generated.h"

class UStaticMeshComponent;

/**
 * 地面に置かれるドーム型(半球)のシールド。
 *
 * ・弾(ヒットスキャン)を止める。外から中・中から外のどちらも止める
 * ・人やグレネードは通り抜ける(当たり判定は持たない)
 * ・Duration 秒たつと消える
 *
 * 弾を止める判定は物理の当たり判定ではなく、
 * 「レイが半球の表面を横切るか」を計算で調べる(FindBlockingShield)。
 * 武器の能力(USavaWeaponFireAbility)がレイを飛ばすときにこれを呼ぶ。
 *
 * 見た目:
 *     初期状態はエンジン付属の球(/Engine/BasicShapes/Sphere)を Radius に合わせて拡大して使う。
 *     下半分は地面に埋まるのでドームに見える。
 *     Blueprint の子クラスで Dome Mesh のマテリアルを半透明のものに変える。
 *
 * Spawn・消去はサーバーだけが行い、全員へ同期する。
 */
UCLASS()
class SAVA_API ASavaDomeShield : public AActor
{
	GENERATED_BODY()

public:
	ASavaDomeShield();

	float GetRadius() const
	{
		return Radius;
	}

	/**
	 * 線分 Start → End が半球(中心 Center の上半分)の表面を横切るか。
	 *
	 * 横切るなら、最初に横切る位置を OutTime(0 = Start、1 = End)に入れて true。
	 * 計算だけの関数(static)なので、テストから直接呼べる。
	 */
	static bool IntersectDome(
		const FVector& Center,
		float DomeRadius,
		const FVector& Start,
		const FVector& End,
		float& OutTime);

	/**
	 * 線分 Start → End を止めるシールドを探す(一番手前のもの)。
	 *
	 * 見つかったら、止まった位置と面の向き(撃った側を向く)を入れて返す。なければ nullptr。
	 */
	static ASavaDomeShield* FindBlockingShield(
		UWorld* World,
		const FVector& Start,
		const FVector& End,
		FVector& OutLocation,
		FVector& OutNormal);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 展開したとき(全員の画面で呼ばれる)。
	 *
	 * Blueprint の子クラスで展開の音・エフェクトを置く。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shield")
	void OnShieldDeployed();

	/**
	 * 消えるとき(全員の画面で呼ばれる)。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shield")
	void OnShieldExpired();

	//========================================
	// Components
	//========================================

	// 見た目だけ(当たり判定なし)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<UStaticMeshComponent> DomeMesh;

	//========================================
	// 設定
	//========================================

	// ドームの半径
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Shield",
		meta = (ClampMin = "50", ForceUnits = "cm"))
	float Radius = 400.0f;

	// 展開している時間(0 なら消えない)
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Shield",
		meta = (ClampMin = "0", ForceUnits = "s"))
	float Duration = 10.0f;

	/**
	 * Dome Mesh の元の半径(拡大率 1 のとき)。
	 *
	 * エンジン付属の球は 50cm。別のメッシュに変えたときはそのメッシュの半径を入れる。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Shield",
		meta = (ClampMin = "1", ForceUnits = "cm"))
	float MeshBaseRadius = 50.0f;

	/**
	 * ドームの範囲を線で表示する(調整用)。
	 *
	 * 配布用(Shipping)ビルドでは表示されない。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Shield|Debug")
	bool bDrawDebug = false;
};
