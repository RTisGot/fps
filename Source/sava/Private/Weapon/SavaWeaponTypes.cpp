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

float FSavaWeaponStats::CalculateMovementSpread(float MoveAlpha, bool bInAir, ESavaSpreadMovement Movement) const
{
	switch (Movement)
	{
	case ESavaSpreadMovement::Walk:    return MovementSpread * MoveAlpha + (bInAir ? JumpSpread : 0.0f);
	case ESavaSpreadMovement::Sprint:  return SprintSpread * MoveAlpha + (bInAir ? JumpSpread : 0.0f);
	case ESavaSpreadMovement::Slide:   return SlideSpread;
	case ESavaSpreadMovement::Grapple: return GrappleSpread;
	}
	return 0.0f;
}

float FSavaWeaponStats::BlendMovementSpread(float Current, float Target, float DeltaTime) const
{
	const float BlendTime = Target > Current ? MovementSpreadInTime : MovementSpreadOutTime;
	if (BlendTime <= 0.0f)
	{
		return Target;
	}

	//だんだんゆっくり近づける(Blend Time で差の 95% が縮む)
	const float Alpha = 1.0f - FMath::Exp(-3.0f * DeltaTime / BlendTime);
	return FMath::Lerp(Current, Target, Alpha);
}

FSavaSpreadBreakdown FSavaWeaponStats::CalculateSpreadBreakdown(float ADSAlpha, float InMovementSpread, float ShotSpread, bool bFirstShot) const
{
	FSavaSpreadBreakdown Result;
	Result.bFirstShot = bFirstShot;
	Result.Base = FMath::Lerp(HipSpread, ADSSpread, ADSAlpha) * (bFirstShot ? FirstShotSpreadScale : 1.0f);

	//ADS に入るほど、移動・空中の拡散と連射の拡散を、それぞれの割合へ減らす
	Result.Movement = InMovementSpread * FMath::Lerp(1.0f, ADSMovementSpreadScale, ADSAlpha);
	Result.Shot = ShotSpread * FMath::Lerp(1.0f, ADSShotSpreadScale, ADSAlpha);
	Result.Total = Result.Base + Result.Movement + Result.Shot;
	return Result;
}

float FSavaWeaponStats::CalculateShotSpread(float ShotSpreadAtLastShot, float TimeSinceLastShot) const
{
	//連射間隔(と Recovery Delay の長い方)が過ぎるまでは戻らない
	const float Delay = FMath::Max(GetFireInterval(), ShotSpreadRecoveryDelay);
	const float RecoveryTime = FMath::Max(TimeSinceLastShot - Delay, 0.0f);
	return FMath::Max(ShotSpreadAtLastShot - ShotSpreadRecovery * RecoveryTime, 0.0f);
}

FVector2D FSavaWeaponStats::SampleSpreadOffset(float SpreadAngle, float RandRadius, float RandAngle) const
{
	return SampleConeOffset(SpreadAngle, SpreadCenterRatio, RandRadius, RandAngle);
}

FVector2D FSavaWeaponStats::SampleConeOffset(float ConeAngle, float CenterRatio, float RandRadius, float RandAngle)
{
	if (ConeAngle <= 0.0f)
	{
		return FVector2D::ZeroVector;
	}

	//半径 = 角度 × 乱数^Exponent。半径の半分より内側に入る割合は 0.5^(1 / Exponent) なので、それが Center Ratio になる Exponent を使う
	//(Center Ratio 0.25 → Exponent 0.5 で円の中に均等、0.5 → 1、0.71 → 2)
	const float Ratio = FMath::Clamp(CenterRatio, 0.01f, 0.99f);
	const float Exponent = FMath::Loge(0.5f) / FMath::Loge(Ratio);
	const float Radius = ConeAngle * FMath::Pow(FMath::Clamp(RandRadius, 0.0f, 1.0f), Exponent);
	const float Angle = RandAngle * UE_TWO_PI;
	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
}

float FSavaWeaponStats::StepADSViewAlpha(float Current, bool bAiming, float DeltaTime) const
{
	if (ADSTime <= 0.0f)
	{
		return bAiming ? 1.0f : 0.0f;
	}

	//進み具合 T(一定の速さ)を、1 - (1 - T)^Ease で見た目の値にする。今の値から T を逆算して進めるので、途中で向きを変えても飛ばない
	const float Ease = FMath::Max(ADSEase, 1.0f);
	auto EaseOut = [Ease](float T) { return 1.0f - FMath::Pow(1.0f - FMath::Clamp(T, 0.0f, 1.0f), Ease); };
	auto InverseEaseOut = [Ease](float Value) { return 1.0f - FMath::Pow(1.0f - FMath::Clamp(Value, 0.0f, 1.0f), 1.0f / Ease); };

	const float Step = DeltaTime / ADSTime;
	if (bAiming)
	{
		return EaseOut(InverseEaseOut(Current) + Step);
	}
	//抜けるときも最初は速く: 「抜けた量」を同じ形で進める
	return 1.0f - EaseOut(InverseEaseOut(1.0f - Current) + Step);
}

float FSavaWeaponStats::GetReloadDuration(bool bEmpty) const
{
	return bEmpty ? EmptyReloadTime : ReloadTime;
}

float FSavaWeaponStats::GetReloadInsertTime(bool bEmpty) const
{
	return GetReloadDuration(bEmpty) * FMath::Clamp(ReloadInsertRatio, 0.0f, 1.0f);
}

int32 FSavaWeaponStats::GetMagazineCapacity(bool bRoundChambered) const
{
	return MagazineSize + (bChamberRound && bRoundChambered ? 1 : 0);
}

FVector2D FSavaWeaponStats::CalculateRecoilKick(int32 ShotIndex, const FVector2D& Noise) const
{
	FVector2D Pattern(0.0f, 1.0f);
	if (RecoilPattern.Num() > 0)
	{
		//最後まで撃ったら Loop Start から繰り返す
		int32 Index = FMath::Max(ShotIndex, 0);
		if (Index >= RecoilPattern.Num())
		{
			const int32 LoopStart = FMath::Clamp(RecoilPatternLoopStart, 0, RecoilPattern.Num() - 1);
			Index = LoopStart + (Index - LoopStart) % (RecoilPattern.Num() - LoopStart);
		}
		Pattern = RecoilPattern[Index];
	}

	const FVector2D Randomized = Pattern + Noise * RecoilRandomness;
	return FVector2D(Randomized.X * HorizontalRecoil, Randomized.Y * VerticalRecoil);
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
		Stats.EmptyReloadTime *= Modifier.ReloadTimeMultiplier;
		Stats.ReloadStartTime *= Modifier.ReloadTimeMultiplier;
		Stats.ReloadRoundTime *= Modifier.ReloadTimeMultiplier;
		Stats.ReloadEndTime *= Modifier.ReloadTimeMultiplier;
		Stats.EquipTime *= Modifier.EquipTimeMultiplier;
		Stats.SprintToFireTime *= Modifier.SprintToFireTimeMultiplier;
		Stats.HipSpread *= Modifier.HipSpreadMultiplier;
		Stats.ADSSpread *= Modifier.ADSSpreadMultiplier;
		//Movement Spread Multiplier は、歩き・ダッシュ・スライディング・グラップルの拡散すべてに掛かる
		Stats.MovementSpread *= Modifier.MovementSpreadMultiplier;
		Stats.SprintSpread *= Modifier.MovementSpreadMultiplier;
		Stats.SlideSpread *= Modifier.MovementSpreadMultiplier;
		Stats.GrappleSpread *= Modifier.MovementSpreadMultiplier;
		Stats.JumpSpread *= Modifier.JumpSpreadMultiplier;
		Stats.ADSMovementSpreadScale *= Modifier.ADSMovementSpreadScaleMultiplier;
		Stats.ADSShotSpreadScale *= Modifier.ADSShotSpreadScaleMultiplier;
		Stats.VerticalRecoil *= Modifier.VerticalRecoilMultiplier;
		Stats.HorizontalRecoil *= Modifier.HorizontalRecoilMultiplier;
		Stats.RecoilRecovery *= Modifier.RecoilRecoveryMultiplier;
		Stats.ADSTime *= Modifier.ADSTimeMultiplier;
		Stats.ADSZoom *= Modifier.ADSZoomMultiplier;
		Stats.ADSMoveSpeed *= Modifier.ADSMoveSpeedMultiplier;
		Stats.FalloffStartDistance *= Modifier.FalloffStartMultiplier;
		Stats.FalloffEndDistance *= Modifier.FalloffEndMultiplier;
		Stats.MinimumDamage *= Modifier.MinimumDamageMultiplier;
		Stats.PelletSpread *= Modifier.PelletSpreadMultiplier;
		Stats.HitScanRadius *= Modifier.HitScanRadiusMultiplier;
	}

	//カスタムで削りすぎても、武器として成り立つ範囲に収める
	Stats.MagazineSize = FMath::Max(Stats.MagazineSize, 1);
	Stats.MaxReserveAmmo = FMath::Max(Stats.MaxReserveAmmo, 0);
	Stats.RateOfFire = FMath::Max(Stats.RateOfFire, 1.0f);
	Stats.ADSZoom = FMath::Clamp(Stats.ADSZoom, 0.05f, 1.0f);
	Stats.ADSMoveSpeed = FMath::Clamp(Stats.ADSMoveSpeed, 0.1f, 1.0f);
	Stats.ADSMovementSpreadScale = FMath::Clamp(Stats.ADSMovementSpreadScale, 0.0f, 1.0f);
	Stats.ADSShotSpreadScale = FMath::Clamp(Stats.ADSShotSpreadScale, 0.0f, 1.0f);
	Stats.ReloadRoundTime = FMath::Max(Stats.ReloadRoundTime, 0.01f);
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
	Modifier.EquipTimeMultiplier = Multiply(End.EquipTimeMultiplier);
	Modifier.SprintToFireTimeMultiplier = Multiply(End.SprintToFireTimeMultiplier);
	Modifier.HipSpreadMultiplier = Multiply(End.HipSpreadMultiplier);
	Modifier.ADSSpreadMultiplier = Multiply(End.ADSSpreadMultiplier);
	Modifier.MovementSpreadMultiplier = Multiply(End.MovementSpreadMultiplier);
	Modifier.JumpSpreadMultiplier = Multiply(End.JumpSpreadMultiplier);
	Modifier.ADSMovementSpreadScaleMultiplier = Multiply(End.ADSMovementSpreadScaleMultiplier);
	Modifier.ADSShotSpreadScaleMultiplier = Multiply(End.ADSShotSpreadScaleMultiplier);
	Modifier.VerticalRecoilMultiplier = Multiply(End.VerticalRecoilMultiplier);
	Modifier.HorizontalRecoilMultiplier = Multiply(End.HorizontalRecoilMultiplier);
	Modifier.RecoilRecoveryMultiplier = Multiply(End.RecoilRecoveryMultiplier);
	Modifier.ADSTimeMultiplier = Multiply(End.ADSTimeMultiplier);
	Modifier.ADSZoomMultiplier = Multiply(End.ADSZoomMultiplier);
	Modifier.ADSMoveSpeedMultiplier = Multiply(End.ADSMoveSpeedMultiplier);
	Modifier.FalloffStartMultiplier = Multiply(End.FalloffStartMultiplier);
	Modifier.FalloffEndMultiplier = Multiply(End.FalloffEndMultiplier);
	Modifier.MinimumDamageMultiplier = Multiply(End.MinimumDamageMultiplier);
	Modifier.PelletSpreadMultiplier = Multiply(End.PelletSpreadMultiplier);
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
