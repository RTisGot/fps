// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponData.h"

namespace SavaWeaponData
{
	//+ 側の効果から、- 側(倍率は逆数、足し算は符号を逆)を作る
	FSavaWeaponModifier Invert(const FSavaWeaponModifier& Modifier)
	{
		auto Inverse = [](float Multiplier) { return Multiplier > 0.0f ? 1.0f / Multiplier : 1.0f; };

		FSavaWeaponModifier Result;
		Result.DamageMultiplier = Inverse(Modifier.DamageMultiplier);
		Result.HeadDamageMultiplier = Inverse(Modifier.HeadDamageMultiplier);
		Result.MagazineSizeAdd = -Modifier.MagazineSizeAdd;
		Result.MaxReserveAmmoAdd = -Modifier.MaxReserveAmmoAdd;
		Result.RateOfFireMultiplier = Inverse(Modifier.RateOfFireMultiplier);
		Result.ReloadTimeMultiplier = Inverse(Modifier.ReloadTimeMultiplier);
		Result.EquipTimeMultiplier = Inverse(Modifier.EquipTimeMultiplier);
		Result.SprintToFireTimeMultiplier = Inverse(Modifier.SprintToFireTimeMultiplier);
		Result.HipSpreadMultiplier = Inverse(Modifier.HipSpreadMultiplier);
		Result.ADSSpreadMultiplier = Inverse(Modifier.ADSSpreadMultiplier);
		Result.MovementSpreadMultiplier = Inverse(Modifier.MovementSpreadMultiplier);
		Result.JumpSpreadMultiplier = Inverse(Modifier.JumpSpreadMultiplier);
		Result.ADSMovementSpreadScaleMultiplier = Inverse(Modifier.ADSMovementSpreadScaleMultiplier);
		Result.ADSShotSpreadScaleMultiplier = Inverse(Modifier.ADSShotSpreadScaleMultiplier);
		Result.VerticalRecoilMultiplier = Inverse(Modifier.VerticalRecoilMultiplier);
		Result.HorizontalRecoilMultiplier = Inverse(Modifier.HorizontalRecoilMultiplier);
		Result.RecoilRecoveryMultiplier = Inverse(Modifier.RecoilRecoveryMultiplier);
		Result.ADSTimeMultiplier = Inverse(Modifier.ADSTimeMultiplier);
		Result.ADSZoomMultiplier = Inverse(Modifier.ADSZoomMultiplier);
		Result.ADSMoveSpeedMultiplier = Inverse(Modifier.ADSMoveSpeedMultiplier);
		Result.FalloffStartMultiplier = Inverse(Modifier.FalloffStartMultiplier);
		Result.FalloffEndMultiplier = Inverse(Modifier.FalloffEndMultiplier);
		Result.MinimumDamageMultiplier = Inverse(Modifier.MinimumDamageMultiplier);
		Result.PelletSpreadMultiplier = Inverse(Modifier.PelletSpreadMultiplier);
		Result.HitScanRadiusMultiplier = Inverse(Modifier.HitScanRadiusMultiplier);
		return Result;
	}

	FSavaWeaponTuningSlider MakeSlider(FName Id, const FText& DisplayName, const FText& MinLabel, const FText& MaxLabel, const FSavaWeaponModifier& MaxModifier)
	{
		FSavaWeaponTuningSlider Slider;
		Slider.Id = Id;
		Slider.DisplayName = DisplayName;
		Slider.MinLabel = MinLabel;
		Slider.MaxLabel = MaxLabel;
		Slider.MaxModifier = MaxModifier;
		Slider.MinModifier = Invert(MaxModifier);
		return Slider;
	}
}

USavaWeaponData::USavaWeaponData()
{
	//おすすめの 6 本(+ 側の効果。- 側はその逆)。武器ごとに数値を変えたり、消したり足したりしてよい
	FSavaWeaponModifier Ammo;
	Ammo.DamageMultiplier = 1.15f;
	Ammo.HeadDamageMultiplier = 1.15f;
	Ammo.MinimumDamageMultiplier = 1.15f;
	Ammo.RateOfFireMultiplier = 1.0f / 1.15f; //ダメージを上げた分だけ連射を遅くして、DPS をそろえる
	TuningSliders.Add(SavaWeaponData::MakeSlider(TEXT("Ammo"), NSLOCTEXT("SavaWeapon", "AmmoName", "弾薬"),
		NSLOCTEXT("SavaWeapon", "AmmoMin", "連射重視"), NSLOCTEXT("SavaWeapon", "AmmoMax", "威力重視"), Ammo));

	FSavaWeaponModifier Magazine;
	Magazine.MagazineSizeAdd = 8; //マガジンの弾数に合わせて武器ごとに変える(30 発なら 8 くらい)
	Magazine.ReloadTimeMultiplier = 1.25f;
	TuningSliders.Add(SavaWeaponData::MakeSlider(TEXT("Magazine"), NSLOCTEXT("SavaWeapon", "MagazineName", "マガジン"),
		NSLOCTEXT("SavaWeapon", "MagazineMin", "早いリロード"), NSLOCTEXT("SavaWeapon", "MagazineMax", "大容量"), Magazine));

	FSavaWeaponModifier Muzzle;
	Muzzle.VerticalRecoilMultiplier = 0.8f;
	Muzzle.HorizontalRecoilMultiplier = 1.25f;
	TuningSliders.Add(SavaWeaponData::MakeSlider(TEXT("Muzzle"), NSLOCTEXT("SavaWeapon", "MuzzleName", "マズル"),
		NSLOCTEXT("SavaWeapon", "MuzzleMin", "横ブレ抑制"), NSLOCTEXT("SavaWeapon", "MuzzleMax", "跳ね上がり抑制"), Muzzle));

	FSavaWeaponModifier Stock;
	Stock.ADSTimeMultiplier = 1.2f;
	Stock.ADSMovementSpreadScaleMultiplier = 0.7f;
	Stock.ADSShotSpreadScaleMultiplier = 0.7f;
	Stock.RecoilRecoveryMultiplier = 1.2f;
	TuningSliders.Add(SavaWeaponData::MakeSlider(TEXT("Stock"), NSLOCTEXT("SavaWeapon", "StockName", "ストック"),
		NSLOCTEXT("SavaWeapon", "StockMin", "素早い構え"), NSLOCTEXT("SavaWeapon", "StockMax", "安定した構え"), Stock));

	FSavaWeaponModifier Barrel;
	Barrel.FalloffStartMultiplier = 1.25f;
	Barrel.FalloffEndMultiplier = 1.25f;
	Barrel.HipSpreadMultiplier = 1.3f;
	TuningSliders.Add(SavaWeaponData::MakeSlider(TEXT("Barrel"), NSLOCTEXT("SavaWeapon", "BarrelName", "バレル"),
		NSLOCTEXT("SavaWeapon", "BarrelMin", "取り回し"), NSLOCTEXT("SavaWeapon", "BarrelMax", "射程"), Barrel));

	FSavaWeaponModifier Frame;
	Frame.ADSSpreadMultiplier = 1.0f / 1.3f;
	Frame.MovementSpreadMultiplier = 1.3f;
	Frame.JumpSpreadMultiplier = 1.3f;
	TuningSliders.Add(SavaWeaponData::MakeSlider(TEXT("Frame"), NSLOCTEXT("SavaWeapon", "FrameName", "フレーム"),
		NSLOCTEXT("SavaWeapon", "FrameMin", "軽量"), NSLOCTEXT("SavaWeapon", "FrameMax", "重量"), Frame));
}

#if WITH_EDITOR
USavaWeaponData::FOnWeaponDataEdited USavaWeaponData::OnWeaponDataEdited;

void USavaWeaponData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnWeaponDataEdited.Broadcast(this);
}
#endif

const FSavaWeaponTuningSlider* USavaWeaponData::FindTuningSlider(FName Id) const
{
	return Id.IsNone() ? nullptr : TuningSliders.FindByPredicate([Id](const FSavaWeaponTuningSlider& Slider) { return Slider.Id == Id; });
}

const FSavaWeaponTuningToggle* USavaWeaponData::FindTuningToggle(FName Id) const
{
	return Id.IsNone() ? nullptr : TuningToggles.FindByPredicate([Id](const FSavaWeaponTuningToggle& Toggle) { return Toggle.Id == Id; });
}

FSavaWeaponTuning USavaWeaponData::SanitizeTuning(const FSavaWeaponTuning& Tuning) const
{
	FSavaWeaponTuning Result;
	for (const FSavaWeaponTuningSlider& Slider : TuningSliders)
	{
		//Id が空・重なっているものは、最初の 1 つだけ使う
		if (FindTuningSlider(Slider.Id) == &Slider)
		{
			Result.SetSliderValue(Slider.Id, USavaWeaponLibrary::SanitizeSliderValue(Tuning.GetSliderValue(Slider.Id)));
		}
	}

	for (const FSavaWeaponTuningToggle& Toggle : TuningToggles)
	{
		if (FindTuningToggle(Toggle.Id) != &Toggle || Toggle.Options.Num() == 0)
		{
			continue;
		}

		const FName Selected = Tuning.GetToggleOption(Toggle.Id);
		const bool bSelectable = !Selected.IsNone() && Toggle.Options.ContainsByPredicate([Selected](const FSavaWeaponTuningOption& Option) { return Option.Id == Selected; });
		Result.SetToggleOption(Toggle.Id, bSelectable ? Selected : Toggle.Options[0].Id);
	}
	return Result;
}

TArray<FSavaWeaponModifier> USavaWeaponData::MakeTuningModifiers(const FSavaWeaponTuning& Tuning) const
{
	const FSavaWeaponTuning Sanitized = SanitizeTuning(Tuning);

	TArray<FSavaWeaponModifier> Modifiers;
	for (const FSavaWeaponTuningSliderValue& Value : Sanitized.Sliders)
	{
		Modifiers.Add(USavaWeaponLibrary::MakeSliderModifier(*FindTuningSlider(Value.Slider), Value.Value));
	}

	for (const FSavaWeaponTuningToggleValue& Value : Sanitized.Toggles)
	{
		const FSavaWeaponTuningToggle* Toggle = FindTuningToggle(Value.Toggle);
		const FSavaWeaponTuningOption* Option = Toggle->Options.FindByPredicate([&Value](const FSavaWeaponTuningOption& Entry) { return Entry.Id == Value.Option; });
		if (Option)
		{
			Modifiers.Add(Option->Modifier);
		}
	}
	return Modifiers;
}

FSavaWeaponStats USavaWeaponData::GetTunedStats(const FSavaWeaponTuning& Tuning) const
{
	return USavaWeaponLibrary::ApplyWeaponModifiers(Stats, MakeTuningModifiers(Tuning));
}
