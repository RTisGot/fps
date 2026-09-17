#pragma once
#include "Commandlets/Commandlet.h"
#include "SavaBuildSettingsMenuCommandlet.generated.h"

/** Authors a real editable Widget Blueprint; never runs in a packaged game. */
UCLASS()
class USavaBuildSettingsMenuCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	USavaBuildSettingsMenuCommandlet();
	virtual int32 Main(const FString& Params) override;
};
