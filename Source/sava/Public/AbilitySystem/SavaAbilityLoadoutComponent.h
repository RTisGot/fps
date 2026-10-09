// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "SavaAbilityLoadoutComponent.generated.h"

class UAbilitySystemComponent;
class USavaGadgetData;
class USavaLoadoutAbilityData;
class USavaSkillData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnSkillChanged, USavaSkillData*, Skill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnGadgetChanged, USavaGadgetData*, Gadget);

//キャラクターが持つスキル・ガジェットを管理するコンポーネント
//・サーバーが持ち、クライアントは追従する
//・能力の付与・取り外しもここがやる。SetSkill / SetGadget を呼ぶだけで、古い能力を外して新しい能力を付ける
//・ASC は PlayerState にあるため、キャラクターが持ち主になったとき・手放したときに GrantAbilities / RevokeAbilities を呼ぶ(AsavaCharacter が呼ぶ)
//・ここで管理するスキル・ガジェットは AbilitySetsには入れない(二重に付与されるため)
UCLASS(ClassGroup = (Sava), meta = (BlueprintSpawnableComponent))
class SAVA_API USavaAbilityLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USavaAbilityLoadoutComponent();

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	static USavaAbilityLoadoutComponent* FindAbilityLoadoutComponent(const AActor* Actor);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//--------------------------------装備(サーバーのみ)

	//スキルを持たせる(ロードアウト画面から呼ぶ想定)。nullptr で外す。付与済みなら能力を入れ替える
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Loadout")
	void SetSkill(USavaSkillData* NewSkill);

	//ガジェットを持たせる。nullptr で外す。付与済みなら能力を入れ替え、個数は満タンになる
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Loadout")
	void SetGadget(USavaGadgetData* NewGadget);

	//--------------------------------状態

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaSkillData* GetSkill() const { return Skill; }

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaGadgetData* GetGadget() const { return Gadget; }

	//持っているスキルが変わった(装備の変更)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Loadout")
	FSavaOnSkillChanged OnSkillChanged;

	//持っているガジェットが変わった(装備の変更)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Loadout")
	FSavaOnGadgetChanged OnGadgetChanged;

	//--------------------------------キャラクターから呼ぶ(サーバーのみ)

	//今のスキル・ガジェットの能力を ASC に付ける。前回付けた分は先に外す
	void GrantAbilities(UAbilitySystemComponent* AbilitySystemComponent);

	//付けた能力を外す(キャラクターが持ち主でなくなるとき)
	void RevokeAbilities();

protected:
	//装備を用意するまでの仮のスキル・ガジェット(ロードアウト画面ができるまで、キャラクターの Blueprint で設定する)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Loadout")
	TObjectPtr<USavaSkillData> DefaultSkill;

	UPROPERTY(EditDefaultsOnly, Category = "Sava|Loadout")
	TObjectPtr<USavaGadgetData> DefaultGadget;

private:
	UFUNCTION()
	void OnRep_Skill();

	UFUNCTION()
	void OnRep_Gadget();

	//1 つの枠の能力を付け直す(前の能力は外す。付与先の ASC がなければ何もしない)
	void GrantSlot(const USavaLoadoutAbilityData* Data, FGameplayAbilitySpecHandle& InOutHandle);
	void ClearSlot(FGameplayAbilitySpecHandle& InOutHandle);

	UPROPERTY(ReplicatedUsing = OnRep_Skill)
	TObjectPtr<USavaSkillData> Skill;

	UPROPERTY(ReplicatedUsing = OnRep_Gadget)
	TObjectPtr<USavaGadgetData> Gadget;

	//能力を付けた先の ASC(付けていなければ null)
	TWeakObjectPtr<UAbilitySystemComponent> GrantedTo;

	FGameplayAbilitySpecHandle SkillHandle;
	FGameplayAbilitySpecHandle GadgetHandle;
};
