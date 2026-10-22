// Fill out your copyright notice in the Description page of Project Settings.

#include "Loadout/SavaLoadoutStatLibrary.h"
#include "Loadout/SavaCharacterTypes.h"
#include "Loadout/SavaLoadoutSettings.h"
#include "Weapon/SavaWeaponData.h"

namespace SavaLoadoutStat
{
	//一番大きい値でもバーがいっぱいにならないようにする余白(カスタマイズで最大より上がっても伸びる分を残す)
	constexpr float Headroom = 1.25f;

	enum class EUnit : uint8
	{
		None,
		Rpm,
		Seconds,
		Meters,
		Degrees,
		DegreesPerSecond,
		MetersPerSecond,
		Multiplier,
	};

	//数値 1 項目の定義
	template <typename T>
	struct TStatDef
	{
		FText Label;
		float (*Get)(const T&);
		bool bHigherIsBetter;
		EUnit Unit;
		int32 Decimals;
		//値の見せ方を変えたいとき(nullptr なら Unit と Decimals で見せる)
		FText (*CustomValueText)(const T&) = nullptr;
	};

	FText FormatValue(float Value, EUnit Unit, int32 Decimals, bool bSigned)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(Decimals).SetMaximumFractionalDigits(Decimals).SetAlwaysSign(bSigned).SetUseGrouping(false);
		const FText Number = FText::AsNumber(Value, &Options);

		switch (Unit)
		{
		case EUnit::Rpm:
			return FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitRpm", "{0} RPM"), Number);
		case EUnit::Seconds:
			return FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitSeconds", "{0}s"), Number);
		case EUnit::Meters:
			return FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitMeters", "{0}m"), Number);
		case EUnit::Degrees:
			return FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitDegrees", "{0}°"), Number);
		case EUnit::DegreesPerSecond:
			return FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitDegreesPerSecond", "{0}°/s"), Number);
		case EUnit::MetersPerSecond:
			return FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitMetersPerSecond", "{0}m/s"), Number);
		case EUnit::Multiplier:
			//増減は「+0.10」のように倍率の記号を付けない
			return bSigned ? Number : FText::Format(NSLOCTEXT("SavaLoadout", "StatUnitMultiplier", "×{0}"), Number);
		default:
			return Number;
		}
	}

	//1 発のダメージ(ショットガンは「12×8」)
	FText FormatPelletDamage(float Damage, int32 PelletCount)
	{
		const FText DamageText = FormatValue(Damage, EUnit::None, 0, false);
		return PelletCount > 1 ? FText::Format(NSLOCTEXT("SavaLoadout", "StatDamagePellet", "{0}×{1}"), DamageText, FText::AsNumber(PelletCount)) : DamageText;
	}

	float CalculateFill(float Value, float Scale, bool bHigherIsBetter)
	{
		if (Scale <= 0.0f)
		{
			return bHigherIsBetter ? 0.0f : 1.0f;
		}
		const float Ratio = Value / Scale;
		return FMath::Clamp(bHigherIsBetter ? Ratio : 1.0f - Ratio, 0.0f, 1.0f);
	}

	template <typename T>
	TArray<FSavaStatRow> MakeRows(TConstArrayView<TStatDef<T>> Defs, const T& Stats, const T& CompareStats, TConstArrayView<T> Candidates)
	{
		TArray<FSavaStatRow> Rows;
		for (const TStatDef<T>& Def : Defs)
		{
			const float Value = Def.Get(Stats);
			const float CompareValue = Def.Get(CompareStats);

			//バーの基準(選べるもの全体と、今見せている 2 つの中の最大)
			float Max = FMath::Max(Value, CompareValue);
			for (const T& Candidate : Candidates)
			{
				Max = FMath::Max(Max, Def.Get(Candidate));
			}
			const float Scale = Max * Headroom;

			auto MakeValueText = [&Def](const T& Source, float SourceValue)
			{
				return Def.CustomValueText ? Def.CustomValueText(Source) : FormatValue(SourceValue, Def.Unit, Def.Decimals, false);
			};

			FSavaStatRow& Row = Rows.AddDefaulted_GetRef();
			Row.Label = Def.Label;
			Row.ValueText = MakeValueText(Stats, Value);
			Row.Fill = CalculateFill(Value, Scale, Def.bHigherIsBetter);
			Row.CompareFill = CalculateFill(CompareValue, Scale, Def.bHigherIsBetter);

			//見せる値が同じなら変化なし(丸めて同じになる細かい差は出さない)
			if (!Row.ValueText.EqualTo(MakeValueText(CompareStats, CompareValue)))
			{
				const float Delta = Value - CompareValue;
				Row.DeltaText = FormatValue(Delta, Def.Unit, Def.Decimals, true);
				Row.Change = (Delta > 0.0f) == Def.bHigherIsBetter ? ESavaStatChange::Better : ESavaStatChange::Worse;
			}
		}
		return Rows;
	}

	const TArray<TStatDef<FSavaWeaponStats>>& GetWeaponStatDefs()
	{
		static const TArray<TStatDef<FSavaWeaponStats>> Defs = {
			//ショットガンは 1 回の合計で比べ、「12×8」のように見せる
			{ NSLOCTEXT("SavaLoadout", "StatDamage", "ダメージ"), [](const FSavaWeaponStats& S) { return S.BaseDamage * S.PelletCount; }, true, EUnit::None, 0,
				[](const FSavaWeaponStats& S) { return FormatPelletDamage(S.BaseDamage, S.PelletCount); } },
			{ NSLOCTEXT("SavaLoadout", "StatRateOfFire", "連射速度"), [](const FSavaWeaponStats& S) { return S.RateOfFire; }, true, EUnit::Rpm, 0 },
			//減衰し始める距離・減衰しきる距離(cm → m)
			{ NSLOCTEXT("SavaLoadout", "StatEffectiveRange", "有効射程"), [](const FSavaWeaponStats& S) { return S.FalloffStartDistance / 100.0f; }, true, EUnit::Meters, 0 },
			{ NSLOCTEXT("SavaLoadout", "StatMaxRange", "最大射程"), [](const FSavaWeaponStats& S) { return S.FalloffEndDistance / 100.0f; }, true, EUnit::Meters, 0 },
			{ NSLOCTEXT("SavaLoadout", "StatMagazine", "マガジン"), [](const FSavaWeaponStats& S) { return static_cast<float>(S.MagazineSize); }, true, EUnit::None, 0 },
			{ NSLOCTEXT("SavaLoadout", "StatReload", "リロード"), [](const FSavaWeaponStats& S) { return S.ReloadTime; }, false, EUnit::Seconds, 1 },
			{ NSLOCTEXT("SavaLoadout", "StatEquipTime", "持ち替え"), [](const FSavaWeaponStats& S) { return S.EquipTime; }, false, EUnit::Seconds, 2 },
			//ダッシュをやめてから撃てるまで
			{ NSLOCTEXT("SavaLoadout", "StatSprintToFireTime", "射撃遅延"), [](const FSavaWeaponStats& S) { return S.SprintToFireTime; }, false, EUnit::Seconds, 2 },
			//ADS に入りきるまでの時間
			{ NSLOCTEXT("SavaLoadout", "StatADSTime", "ADS速度"), [](const FSavaWeaponStats& S) { return S.ADSTime; }, false, EUnit::Seconds, 2 },
			//視野角の倍率を、ズームの倍率にして見せる(0.8 → ×1.25)
			{ NSLOCTEXT("SavaLoadout", "StatADSZoom", "ズーム倍率"), [](const FSavaWeaponStats& S) { return S.ADSZoom > 0.0f ? 1.0f / S.ADSZoom : 1.0f; }, true, EUnit::Multiplier, 2 },
			{ NSLOCTEXT("SavaLoadout", "StatVerticalRecoil", "縦反動"), [](const FSavaWeaponStats& S) { return S.VerticalRecoil; }, false, EUnit::Degrees, 2 },
			{ NSLOCTEXT("SavaLoadout", "StatHorizontalRecoil", "横反動"), [](const FSavaWeaponStats& S) { return S.HorizontalRecoil; }, false, EUnit::Degrees, 2 },
			//撃ち終わった後に跳ね上がりが戻る速さ
			{ NSLOCTEXT("SavaLoadout", "StatRecoilRecovery", "反動制御"), [](const FSavaWeaponStats& S) { return S.RecoilRecovery; }, true, EUnit::DegreesPerSecond, 1 },
			//腰撃ちの拡散
			{ NSLOCTEXT("SavaLoadout", "StatHipSpread", "基本拡散"), [](const FSavaWeaponStats& S) { return S.HipSpread; }, false, EUnit::Degrees, 1 },
			{ NSLOCTEXT("SavaLoadout", "StatADSSpread", "ADS拡散"), [](const FSavaWeaponStats& S) { return S.ADSSpread; }, false, EUnit::Degrees, 2 },
		};
		return Defs;
	}

	const TArray<TStatDef<FSavaCharacterRow>>& GetCharacterStatDefs()
	{
		static const TArray<TStatDef<FSavaCharacterRow>> Defs = {
			{ NSLOCTEXT("SavaLoadout", "StatMaxHP", "HP"), [](const FSavaCharacterRow& R) { return R.MaxHP; }, true, EUnit::None, 0 },
			//cm/s → m/s
			{ NSLOCTEXT("SavaLoadout", "StatMoveSpeed", "移動速度"), [](const FSavaCharacterRow& R) { return R.MoveSpeed / 100.0f; }, true, EUnit::MetersPerSecond, 1 },
			{ NSLOCTEXT("SavaLoadout", "StatDamageMultiplier", "与ダメージ"), [](const FSavaCharacterRow& R) { return R.DamageMultiplier; }, true, EUnit::Multiplier, 2 },
		};
		return Defs;
	}

	FSavaCharacterRow FindCharacterRowOrDefault(FName CharacterId)
	{
		const FSavaCharacterRow* Row = USavaLoadoutSettings::FindCharacterRow(CharacterId);
		return Row ? *Row : FSavaCharacterRow();
	}
}

TArray<FSavaStatRow> USavaLoadoutStatLibrary::MakeWeaponStatRows(const FSavaWeaponStats& Stats, const FSavaWeaponStats& CompareStats)
{
	//メイン・サブを同じ基準で比べる(サブ武器のバーが短く見えるのはそのまま)
	TArray<FSavaWeaponStats> Candidates;
	for (const ESavaWeaponSlot Slot : { ESavaWeaponSlot::Primary, ESavaWeaponSlot::Secondary })
	{
		for (const USavaWeaponData* Weapon : USavaLoadoutSettings::GetSelectableWeapons(Slot))
		{
			Candidates.Add(Weapon->Stats);
		}
	}
	return SavaLoadoutStat::MakeRows<FSavaWeaponStats>(SavaLoadoutStat::GetWeaponStatDefs(), Stats, CompareStats, Candidates);
}

TArray<FSavaStatRow> USavaLoadoutStatLibrary::MakeCharacterStatRows(FName Character, FName CompareCharacter)
{
	TArray<FSavaCharacterRow> Candidates;
	for (const FName CharacterId : USavaLoadoutSettings::GetSelectableCharacters())
	{
		Candidates.Add(SavaLoadoutStat::FindCharacterRowOrDefault(CharacterId));
	}
	return SavaLoadoutStat::MakeRows<FSavaCharacterRow>(SavaLoadoutStat::GetCharacterStatDefs(),
		SavaLoadoutStat::FindCharacterRowOrDefault(Character), SavaLoadoutStat::FindCharacterRowOrDefault(CompareCharacter), Candidates);
}
