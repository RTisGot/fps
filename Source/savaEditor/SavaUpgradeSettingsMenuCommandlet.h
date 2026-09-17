#pragma once
#include "Commandlets/Commandlet.h"
#include "SavaUpgradeSettingsMenuCommandlet.generated.h"

/** One-time, non-destructive migration of the existing Designer tree. */
UCLASS()
class USavaUpgradeSettingsMenuCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	USavaUpgradeSettingsMenuCommandlet();
	virtual int32 Main(const FString& Params) override;
};
