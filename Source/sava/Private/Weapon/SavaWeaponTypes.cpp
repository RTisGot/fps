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

//--------------------------------カスタマイズ

namespace SavaWeaponTuning
{
	//スライダーを + の端まで振ったときの倍率(- の端ではこの逆数になる)
	constexpr float AmmoDamage = 1.15f;
	constexpr float MagazineSizeRatio = 0.25f; //マガジンの弾数に対する増減の割合
	constexpr float MagazineReloadTime = 1.25f;
	constexpr float MuzzleVerticalRecoil = 0.8f;
	constexpr float MuzzleHorizontalRecoil = 1.25f;
	constexpr float StockADSTime = 1.2f;
	constexpr float StockADSMovementSpreadScale = 0.7f;
	constexpr float StockRecoilRecovery = 1.2f;
	constexpr float BarrelFalloff = 1.25f;
	constexpr float BarrelHipSpread = 1.3f;
	constexpr float FrameMovementSpread = 1.3f;
	constexpr float FrameADSSpread = 1.0f / 1.3f;

	template <typename T>
	auto& GetField(T& Values, ESavaWeaponTuning Tuning)
	{
		switch (Tuning)
		{
		case ESavaWeaponTuning::Magazine: return Values.Magazine;
		case ESavaWeaponTuning::Muzzle: return Values.Muzzle;
		case ESavaWeaponTuning::Stock: return Values.Stock;
		case ESavaWeaponTuning::Barrel: return Values.Barrel;
		case ESavaWeaponTuning::Frame: return Values.Frame;
		default: return Values.Ammo;
		}
	}

	float SanitizeValue(float Value)
	{
		if (!FMath::IsFinite(Value))
		{
			return 0.0f;
		}
		return FMath::RoundToFloat(FMath::Clamp(Value, -1.0f, 1.0f) * 100.0f) / 100.0f;
	}
}

float FSavaWeaponTuning::GetValue(ESavaWeaponTuning Tuning) const
{
	return SavaWeaponTuning::GetField(*this, Tuning);
}

void FSavaWeaponTuning::SetValue(ESavaWeaponTuning Tuning, float Value)
{
	SavaWeaponTuning::GetField(*this, Tuning) = Value;
}

FSavaWeaponTuning FSavaWeaponTuning::GetSanitized() const
{
	FSavaWeaponTuning Result;
	Result.Ammo = SavaWeaponTuning::SanitizeValue(Ammo);
	Result.Magazine = SavaWeaponTuning::SanitizeValue(Magazine);
	Result.Muzzle = SavaWeaponTuning::SanitizeValue(Muzzle);
	Result.Stock = SavaWeaponTuning::SanitizeValue(Stock);
	Result.Barrel = SavaWeaponTuning::SanitizeValue(Barrel);
	Result.Frame = SavaWeaponTuning::SanitizeValue(Frame);
	return Result;
}

bool FSavaWeaponTuning::operator==(const FSavaWeaponTuning& Other) const
{
	return Ammo == Other.Ammo
		&& Magazine == Other.Magazine
		&& Muzzle == Other.Muzzle
		&& Stock == Other.Stock
		&& Barrel == Other.Barrel
		&& Frame == Other.Frame;
}

float FSavaWeaponTuningStrength::GetValue(ESavaWeaponTuning Tuning) const
{
	return SavaWeaponTuning::GetField(*this, Tuning);
}

FSavaWeaponModifier USavaWeaponLibrary::MakeTuningModifier(const FSavaWeaponStats& BaseStats, const FSavaWeaponTuning& Tuning, const FSavaWeaponTuningStrength& Strength)
{
	using namespace SavaWeaponTuning;
	const FSavaWeaponTuning Values = Tuning.GetSanitized();

	//スライダーの値 × 武器ごとの強さ。倍率は「端の倍率 ^ これ」にする(+1 で端の倍率、-1 でその逆数、0 で変化なし)
	auto GetAmount = [&Values, &Strength](ESavaWeaponTuning Slider)
	{
		return Values.GetValue(Slider) * FMath::Max(Strength.GetValue(Slider), 0.0f);
	};

	FSavaWeaponModifier Modifier;

	//弾薬: 1 発のダメージ(頭・最低ダメージも同じ割合)を上げた分だけ連射を遅くする
	const float Ammo = GetAmount(ESavaWeaponTuning::Ammo);
	const float DamageScale = FMath::Pow(AmmoDamage, Ammo);
	Modifier.DamageMultiplier = DamageScale;
	Modifier.HeadDamageMultiplier = DamageScale;
	Modifier.MinimumDamageMultiplier = DamageScale;
	Modifier.RateOfFireMultiplier = 1.0f / DamageScale;

	const float Magazine = GetAmount(ESavaWeaponTuning::Magazine);
	//0.5 は 0 から遠い方へ丸める(+ と - で増減の数をそろえるため)
	Modifier.MagazineSizeAdd = static_cast<int32>(FMath::RoundHalfFromZero(BaseStats.MagazineSize * MagazineSizeRatio * Magazine));
	Modifier.ReloadTimeMultiplier = FMath::Pow(MagazineReloadTime, Magazine);

	const float Muzzle = GetAmount(ESavaWeaponTuning::Muzzle);
	Modifier.VerticalRecoilMultiplier = FMath::Pow(MuzzleVerticalRecoil, Muzzle);
	Modifier.HorizontalRecoilMultiplier = FMath::Pow(MuzzleHorizontalRecoil, Muzzle);

	const float Stock = GetAmount(ESavaWeaponTuning::Stock);
	Modifier.ADSTimeMultiplier = FMath::Pow(StockADSTime, Stock);
	Modifier.ADSMovementSpreadScaleMultiplier = FMath::Pow(StockADSMovementSpreadScale, Stock);
	Modifier.RecoilRecoveryMultiplier = FMath::Pow(StockRecoilRecovery, Stock);

	const float Barrel = GetAmount(ESavaWeaponTuning::Barrel);
	Modifier.FalloffStartMultiplier = FMath::Pow(BarrelFalloff, Barrel);
	Modifier.FalloffEndMultiplier = FMath::Pow(BarrelFalloff, Barrel);
	Modifier.HipSpreadMultiplier = FMath::Pow(BarrelHipSpread, Barrel);

	const float Frame = GetAmount(ESavaWeaponTuning::Frame);
	Modifier.MovementSpreadMultiplier = FMath::Pow(FrameMovementSpread, Frame);
	Modifier.JumpSpreadMultiplier = FMath::Pow(FrameMovementSpread, Frame);
	Modifier.ADSSpreadMultiplier = FMath::Pow(FrameADSSpread, Frame);
	return Modifier;
}

FSavaWeaponTuning USavaWeaponLibrary::SetTuningValue(FSavaWeaponTuning Tuning, ESavaWeaponTuning Slider, float Value)
{
	Tuning.SetValue(Slider, Value);
	return Tuning;
}
