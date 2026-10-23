#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

//武器データ(USavaWeaponData)の詳細パネル
//Stats の項目を、項目の Category(「Spread|Movement」なら 拡散 → 移動・空中)ごとのカテゴリ・グループに分けて表示する
//並び: Weapon → Stats の各カテゴリ → Tuning → Visual
class FSavaWeaponDataCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

//武器のカスタム(FSavaWeaponModifier)の表示。項目の Category ごとに、閉じたグループに分ける
class FSavaWeaponModifierCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
};
