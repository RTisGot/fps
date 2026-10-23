#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SavaWeaponDataCustomization.h"
#include "Weapon/SavaWeaponData.h"

class FSavaEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		//武器データの詳細パネルを、カテゴリ・グループに分けて見やすくする
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(USavaWeaponData::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FSavaWeaponDataCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FSavaWeaponModifier::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSavaWeaponModifierCustomization::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyModule->UnregisterCustomClassLayout(USavaWeaponData::StaticClass()->GetFName());
			PropertyModule->UnregisterCustomPropertyTypeLayout(FSavaWeaponModifier::StaticStruct()->GetFName());
			PropertyModule->NotifyCustomizationModuleChanged();
		}
	}
};

IMPLEMENT_MODULE(FSavaEditorModule, savaEditor)
