// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponTypes.h"

float FSavaWeaponStats::GetFireInterval() const
{
	return 60.0f / FMath::Max(RateOfFire, 1.0f);
}

float FSavaWeaponStats::CalculateDamage(float Distance, bool bHeadshot) const
{
	//距離による減衰の進み具合(0 = 減衰なし、1 = 最低ダメージまで下がりきった)
	float FalloffAlpha = 0.0f;
	if (FalloffEndDistance > FalloffStartDistance)
	{
		FalloffAlpha = FMath::Clamp((Distance - FalloffStartDistance) / (FalloffEndDistance - FalloffStartDistance), 0.0f, 1.0f);
	}
	else if (Distance > FalloffStartDistance)
	{
		FalloffAlpha = 1.0f;
	}

	const float BodyDamage = FMath::Lerp(BaseDamage, FMath::Min(MinimumDamage, BaseDamage), FalloffAlpha);
	if (!bHeadshot)
	{
		return BodyDamage;
	}
	if (BaseDamage <= 0.0f)
	{
		return HeadDamage;
	}

	//頭は、胴体と同じ割合だけ減衰させる
	return HeadDamage * (BodyDamage / BaseDamage);
}

float FSavaWeaponStats::CalculateSpread(float ADSAlpha, float MoveAlpha, bool bInAir) const
{
	const float BaseSpread = FMath::Lerp(HipSpread, ADSSpread, ADSAlpha);
	const float MovementPart = MovementSpread * MoveAlpha + (bInAir ? JumpSpread : 0.0f);

	//ADS に入るほど、移動中・空中の拡散を ADS Movement Spread Scale の割合へ減らす
	const float MovementScale = FMath::Lerp(1.0f, ADSMovementSpreadScale, ADSAlpha);
	return BaseSpread + MovementPart * MovementScale;
}

FSavaWeaponStats USavaWeaponLibrary::ApplyWeaponModifiers(const FSavaWeaponStats& BaseStats, const TArray<FSavaWeaponModifier>& Modifiers)
{
	FSavaWeaponStats Stats = BaseStats;
	for (const FSavaWeaponModifier& Modifier : Modifiers)
	{
		Stats.BaseDamage *= Modifier.DamageMultiplier;
		Stats.HeadDamage *= Modifier.HeadDamageMultiplier;
		Stats.MagazineSize += Modifier.MagazineSizeAdd;
		Stats.MaxReserveAmmo += Modifier.MaxReserveAmmoAdd;
		Stats.RateOfFire *= Modifier.RateOfFireMultiplier;
		Stats.ReloadTime *= Modifier.ReloadTimeMultiplier;
		Stats.HipSpread *= Modifier.HipSpreadMultiplier;
		Stats.ADSSpread *= Modifier.ADSSpreadMultiplier;
		Stats.MovementSpread *= Modifier.MovementSpreadMultiplier;
		Stats.JumpSpread *= Modifier.JumpSpreadMultiplier;
		Stats.ADSMovementSpreadScale *= Modifier.ADSMovementSpreadScaleMultiplier;
		Stats.VerticalRecoil *= Modifier.VerticalRecoilMultiplier;
		Stats.HorizontalRecoil *= Modifier.HorizontalRecoilMultiplier;
		Stats.RecoilRecovery *= Modifier.RecoilRecoveryMultiplier;
		Stats.ADSTime *= Modifier.ADSTimeMultiplier;
		Stats.ADSZoom *= Modifier.ADSZoomMultiplier;
		Stats.FalloffStartDistance *= Modifier.FalloffStartMultiplier;
		Stats.FalloffEndDistance *= Modifier.FalloffEndMultiplier;
		Stats.MinimumDamage *= Modifier.MinimumDamageMultiplier;
		Stats.HitScanRadius *= Modifier.HitScanRadiusMultiplier;
	}

	//カスタムで削りすぎても、武器として成り立つ範囲に収める
	Stats.MagazineSize = FMath::Max(Stats.MagazineSize, 1);
	Stats.MaxReserveAmmo = FMath::Max(Stats.MaxReserveAmmo, 0);
	Stats.RateOfFire = FMath::Max(Stats.RateOfFire, 1.0f);
	Stats.ADSZoom = FMath::Clamp(Stats.ADSZoom, 0.05f, 1.0f);
	Stats.ADSMovementSpreadScale = FMath::Clamp(Stats.ADSMovementSpreadScale, 0.0f, 1.0f);
	return Stats;
}
