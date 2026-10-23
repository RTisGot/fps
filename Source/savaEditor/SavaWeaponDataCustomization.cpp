#include "SavaWeaponDataCustomization.h"
#include "Weapon/SavaWeaponData.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"

namespace SavaWeaponDetails
{
	//Category の名前 → 画面に出す名前(無いものはそのまま出す)
	FText GetDisplayName(const FString& Category)
	{
		static const TMap<FString, FText> Names = {
			{ TEXT("Damage"),   NSLOCTEXT("SavaWeaponDetails", "Damage", "ダメージ") },
			{ TEXT("Ammo"),     NSLOCTEXT("SavaWeaponDetails", "Ammo", "弾数・連射") },
			{ TEXT("Reload"),   NSLOCTEXT("SavaWeaponDetails", "Reload", "リロード") },
			{ TEXT("Spread"),   NSLOCTEXT("SavaWeaponDetails", "Spread", "拡散") },
			{ TEXT("Base"),     NSLOCTEXT("SavaWeaponDetails", "SpreadBase", "基本") },
			{ TEXT("Movement"), NSLOCTEXT("SavaWeaponDetails", "SpreadMovement", "移動・空中") },
			{ TEXT("Shot"),     NSLOCTEXT("SavaWeaponDetails", "SpreadShot", "連射") },
			{ TEXT("Recoil"),   NSLOCTEXT("SavaWeaponDetails", "Recoil", "反動") },
			{ TEXT("Visual"),   NSLOCTEXT("SavaWeaponDetails", "RecoilVisual", "見た目の揺れ") },
			{ TEXT("ADS"),      NSLOCTEXT("SavaWeaponDetails", "ADS", "ADS") },
			{ TEXT("Falloff"),  NSLOCTEXT("SavaWeaponDetails", "Falloff", "距離減衰") },
			{ TEXT("Pellet"),   NSLOCTEXT("SavaWeaponDetails", "Pellet", "弾の数・当たり判定") },
		};
		const FText* Found = Names.Find(Category);
		return Found ? *Found : FText::FromString(Category);
	}

	//「A|B」を A と B に分ける(B が無ければ空)
	void SplitCategory(const TSharedRef<IPropertyHandle>& Child, FString& OutTop, FString& OutSub)
	{
		const FProperty* Property = Child->GetProperty();
		const FString Category = Property ? Property->GetMetaData(TEXT("Category")) : FString();
		if (!Category.Split(TEXT("|"), &OutTop, &OutSub))
		{
			OutTop = Category;
			OutSub.Reset();
		}
	}

	TArray<TSharedRef<IPropertyHandle>> GetChildren(const TSharedRef<IPropertyHandle>& Handle)
	{
		TArray<TSharedRef<IPropertyHandle>> Children;
		uint32 NumChildren = 0;
		Handle->GetNumChildren(NumChildren);
		for (uint32 Index = 0; Index < NumChildren; ++Index)
		{
			if (TSharedPtr<IPropertyHandle> Child = Handle->GetChildHandle(Index))
			{
				Children.Add(Child.ToSharedRef());
			}
		}
		return Children;
	}
}

//--------------------------------USavaWeaponData

TSharedRef<IDetailCustomization> FSavaWeaponDataCustomization::MakeInstance()
{
	return MakeShared<FSavaWeaponDataCustomization>();
}

void FSavaWeaponDataCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace SavaWeaponDetails;

	const TSharedRef<IPropertyHandle> StatsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USavaWeaponData, Stats));
	if (!StatsHandle->IsValidHandle())
	{
		return;
	}
	DetailBuilder.HideProperty(StatsHandle);

	//Stats の項目を、Category ごとのカテゴリへ(「A|B」は A のカテゴリの中の B のグループ)
	TArray<FName> StatCategories;
	TMap<FString, IDetailGroup*> Groups;
	for (const TSharedRef<IPropertyHandle>& Child : GetChildren(StatsHandle))
	{
		FString Top;
		FString Sub;
		SplitCategory(Child, Top, Sub);

		const FName CategoryName(*FString::Printf(TEXT("Stats_%s"), *Top));
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, GetDisplayName(Top));
		StatCategories.AddUnique(CategoryName);

		if (Sub.IsEmpty())
		{
			Category.AddProperty(Child);
			continue;
		}

		const FString GroupKey = Top + TEXT("|") + Sub;
		IDetailGroup*& Group = Groups.FindOrAdd(GroupKey);
		if (!Group)
		{
			Group = &Category.AddGroup(FName(*GroupKey), GetDisplayName(Sub), false, true);
		}
		Group->AddPropertyRow(Child);
	}

	//Weapon → Stats の各カテゴリ(項目の順) → それ以外(Tuning・Visual)の順に並べる
	DetailBuilder.SortCategories([StatCategories](const TMap<FName, IDetailCategoryBuilder*>& AllCategories)
	{
		int32 Order = 0;
		if (IDetailCategoryBuilder* const* Weapon = AllCategories.Find(TEXT("Weapon")))
		{
			(*Weapon)->SetSortOrder(Order++);
		}
		for (const FName& Name : StatCategories)
		{
			if (IDetailCategoryBuilder* const* Category = AllCategories.Find(Name))
			{
				(*Category)->SetSortOrder(Order++);
			}
		}
		for (const TPair<FName, IDetailCategoryBuilder*>& Pair : AllCategories)
		{
			if (Pair.Key != TEXT("Weapon") && !StatCategories.Contains(Pair.Key))
			{
				Pair.Value->SetSortOrder(Order + Pair.Value->GetSortOrder());
			}
		}
	});
}

//--------------------------------FSavaWeaponModifier

TSharedRef<IPropertyTypeCustomization> FSavaWeaponModifierCustomization::MakeInstance()
{
	return MakeShared<FSavaWeaponModifierCustomization>();
}

void FSavaWeaponModifierCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
}

void FSavaWeaponModifierCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	using namespace SavaWeaponDetails;

	//スライダー 1 本に 2 つずつあって長いので、Category ごとの閉じたグループにする
	TMap<FString, IDetailGroup*> Groups;
	for (const TSharedRef<IPropertyHandle>& Child : GetChildren(PropertyHandle))
	{
		FString Top;
		FString Sub;
		SplitCategory(Child, Top, Sub);

		IDetailGroup*& Group = Groups.FindOrAdd(Top);
		if (!Group)
		{
			Group = &ChildBuilder.AddGroup(FName(*Top), GetDisplayName(Top));
		}
		Group->AddPropertyRow(Child);
	}
}
