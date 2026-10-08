#pragma once

#include "CoreMinimal.h"
#include "Gadget/GadgetBase.h"
#include "GasSmokeGrenade.generated.h"

class AGasSmokeArea;

UCLASS()
class SAVA_API AGasSmokeGrenade : public AGadgetBase
{
	GENERATED_BODY()

public:
	AGasSmokeGrenade();

protected:
	virtual void OnGadgetLanded(const FHitResult& Hit) override;
	virtual void ApplyGadgetEffect() override;

	/**
	 * ガス煙エリアを生成した際に呼ばれる。
	 * VFX / SFXなどの演出用。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|GasSmoke")
	void OnGasSmokeStarted(const FVector& SmokeLocation);

	/**
	 * ガス煙エリアのクラス。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Gadget|GasSmoke",
		meta = (DisplayName = "Gas Smoke Area Class"))
	TSubclassOf<AGasSmokeArea> m_GasSmokeAreaClass;

	/**
	 * 1秒あたりのダメージ量。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Gadget|GasSmoke",
		meta = (DisplayName = "Damage Per Second", ClampMin = "0"))
	float m_DamagePerSecond = 3.0f;

	/**
	 * 自分自身にダメージを与えるか。
	 *
	 * false : 自分にはダメージを与えない
	 * true  : 自分にもダメージを与える
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Gadget|GasSmoke",
		meta = (DisplayName = "Affect Owner"))
	bool m_bAffectOwner = false;
};