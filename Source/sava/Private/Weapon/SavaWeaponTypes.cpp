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

float FSavaWeaponTuning::GetSliderValue(FName Slider) const
{
	const FSavaWeaponTuningSliderValue* Found = Sliders.FindByPredicate([Slider](const FSavaWeaponTuningSliderValue& Entry) { return Entry.Slider == Slider; });
	return Found ? Found->Value : 0.0f;
}

void FSavaWeaponTuning::SetSliderValue(FName Slider, float Value)
{
	if (FSavaWeaponTuningSliderValue* Found = Sliders.FindByPredicate([Slider](const FSavaWeaponTuningSliderValue& Entry) { return Entry.Slider == Slider; }))
	{
		Found->Value = Value;
		return;
	}
	FSavaWeaponTuningSliderValue& Added = Sliders.AddDefaulted_GetRef();
	Added.Slider = Slider;
	Added.Value = Value;
}

FName FSavaWeaponTuning::GetToggleOption(FName Toggle) const
{
	const FSavaWeaponTuningToggleValue* Found = Toggles.FindByPredicate([Toggle](const FSavaWeaponTuningToggleValue& Entry) { return Entry.Toggle == Toggle; });
	return Found ? Found->Option : NAME_None;
}

void FSavaWeaponTuning::SetToggleOption(FName Toggle, FName Option)
{
	if (FSavaWeaponTuningToggleValue* Found = Toggles.FindByPredicate([Toggle](const FSavaWeaponTuningToggleValue& Entry) { return Entry.Toggle == Toggle; }))
	{
		Found->Option = Option;
		return;
	}
	FSavaWeaponTuningToggleValue& Added = Toggles.AddDefaulted_GetRef();
	Added.Toggle = Toggle;
	Added.Option = Option;
}

FSavaWeaponModifier USavaWeaponLibrary::MakeSliderModifier(const FSavaWeaponTuningSlider& Slider, float Value)
{
	const float Amount = FMath::Abs(SanitizeSliderValue(Value));
	const FSavaWeaponModifier& End = Value >= 0.0f ? Slider.MaxModifier : Slider.MinModifier;

	//掛け算は対数で補間する(0.5 なら倍率の平方根。1.2 倍と 1/1.2 倍が同じ強さになる)
	auto Multiply = [Amount](float EndMultiplier) { return FMath::Pow(FMath::Max(EndMultiplier, 0.0f), Amount); };
	//足し算は 0.5 を 0 から遠い方へ丸める(+ と - で増減の数をそろえるため)
	auto Add = [Amount](int32 EndAdd) { return static_cast<int32>(FMath::RoundHalfFromZero(EndAdd * Amount)); };

	FSavaWeaponModifier Modifier;
	Modifier.DamageMultiplier = Multiply(End.DamageMultiplier);
	Modifier.HeadDamageMultiplier = Multiply(End.HeadDamageMultiplier);
	Modifier.MagazineSizeAdd = Add(End.MagazineSizeAdd);
	Modifier.MaxReserveAmmoAdd = Add(End.MaxReserveAmmoAdd);
	Modifier.RateOfFireMultiplier = Multiply(End.RateOfFireMultiplier);
	Modifier.ReloadTimeMultiplier = Multiply(End.ReloadTimeMultiplier);
	Modifier.HipSpreadMultiplier = Multiply(End.HipSpreadMultiplier);
	Modifier.ADSSpreadMultiplier = Multiply(End.ADSSpreadMultiplier);
	Modifier.MovementSpreadMultiplier = Multiply(End.MovementSpreadMultiplier);
	Modifier.JumpSpreadMultiplier = Multiply(End.JumpSpreadMultiplier);
	Modifier.ADSMovementSpreadScaleMultiplier = Multiply(End.ADSMovementSpreadScaleMultiplier);
	Modifier.VerticalRecoilMultiplier = Multiply(End.VerticalRecoilMultiplier);
	Modifier.HorizontalRecoilMultiplier = Multiply(End.HorizontalRecoilMultiplier);
	Modifier.RecoilRecoveryMultiplier = Multiply(End.RecoilRecoveryMultiplier);
	Modifier.ADSTimeMultiplier = Multiply(End.ADSTimeMultiplier);
	Modifier.ADSZoomMultiplier = Multiply(End.ADSZoomMultiplier);
	Modifier.FalloffStartMultiplier = Multiply(End.FalloffStartMultiplier);
	Modifier.FalloffEndMultiplier = Multiply(End.FalloffEndMultiplier);
	Modifier.MinimumDamageMultiplier = Multiply(End.MinimumDamageMultiplier);
	Modifier.HitScanRadiusMultiplier = Multiply(End.HitScanRadiusMultiplier);
	return Modifier;
}

float USavaWeaponLibrary::SanitizeSliderValue(float Value)
{
	if (!FMath::IsFinite(Value))
	{
		return 0.0f;
	}
	return FMath::RoundToFloat(FMath::Clamp(Value, -1.0f, 1.0f) * 100.0f) / 100.0f;
}

FSavaWeaponTuning USavaWeaponLibrary::SetTuningSliderValue(FSavaWeaponTuning Tuning, FName Slider, float Value)
{
	Tuning.SetSliderValue(Slider, Value);
	return Tuning;
}

FSavaWeaponTuning USavaWeaponLibrary::SetTuningToggleOption(FSavaWeaponTuning Tuning, FName Toggle, FName Option)
{
	Tuning.SetToggleOption(Toggle, Option);
	return Tuning;
}
