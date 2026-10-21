// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/SavaLoadoutTypes.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaLoadoutSubsystem.generated.h"

class USavaCharacterOption;
class USavaGadgetData;
class USavaSkillData;
class USavaWeaponData;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnLocalLoadoutChanged, const FSavaLoadout&, Loadout);

//自分が選んだロードアウトを持つ(自分の PC だけの情報。マップを移っても残る)
//・起動時にディスクから読み、変えるたびに保存する
//・変わると OnLoadoutChanged が呼ばれ、ASavaPlayerController がサーバーへ送る
//ロードアウト画面はここを読み書きするだけでよい
UCLASS()
class SAVA_API USavaLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	const FSavaLoadout& GetLoadout() const { return Loadout; }

	//--------------------------------変更(画面から呼ぶ)

	//DT_Character の行名で選ぶ
	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetCharacter(FName CharacterId);

	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetWeapon(ESavaWeaponSlot Slot, USavaWeaponData* Weapon);

	//武器のカスタマイズ(スライダーの値・切り替えの選択)を変える。武器ごとに覚えておき、武器を選び直すと前の値に戻る
	//変えるたびに保存してサーバーへ送るので、スライダーを動かしている最中ではなく離したときに呼ぶ
	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetWeaponTuning(ESavaWeaponSlot Slot, const FSavaWeaponTuning& Tuning);

	//スライダー 1 本分だけ変える(Slider は武器データの Tuning Sliders の Id)
	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetWeaponTuningSliderValue(ESavaWeaponSlot Slot, FName Slider, float Value);

	//切り替え 1 つ分だけ変える(Toggle・Option は武器データの Tuning Toggles の Id)
	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetWeaponTuningOption(ESavaWeaponSlot Slot, FName Toggle, FName Option);

	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetSkill(USavaSkillData* Skill);

	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetGadget(USavaGadgetData* Gadget);

	//--------------------------------一覧の 1 項目用(キャラクター・武器・スキル・ガジェットのどれでも渡せる)

	//その項目を選ぶ(武器はデータの Slot の枠に入る)
	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SelectItem(UObject* Item);

	//その項目を今選んでいるか
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	bool IsSelected(const UObject* Item) const;

	//その項目の表示名とアイコン
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	static void GetItemDisplayInfo(const UObject* Item, FText& DisplayName, UTexture2D*& Icon);

	//--------------------------------今選んでいるもの(読み込み済みのアセット)

	//DT_Character の行名
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	FName GetCharacter() const { return Loadout.Character; }

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaWeaponData* GetWeapon(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	FSavaWeaponTuning GetWeaponTuning(ESavaWeaponSlot Slot) const;

	//カスタマイズを適用した武器の数値(画面で数値を見せる用。武器がなければ初期値)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	FSavaWeaponStats GetTunedWeaponStats(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaSkillData* GetSkill() const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaGadgetData* GetGadget() const;

	//--------------------------------選べるもの(Project Settings → Game → Sava Loadout)

	//キャラクターの一覧。武器などと同じく、1 項目ずつ SelectItem / IsSelected / GetItemDisplayInfo に渡せる
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaCharacterOption*> GetSelectableCharacters();

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaWeaponData*> GetSelectableWeapons(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaSkillData*> GetSelectableSkills() const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaGadgetData*> GetSelectableGadgets() const;

	//--------------------------------枠の種類で扱う(ロードアウト画面の 1 つの部品で、どの枠でも扱えるようにする)

	//その枠で選べるもの。1 項目ずつ SelectItem / IsSelected / GetItemDisplayInfo に渡せる
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<UObject*> GetSelectableItems(ESavaLoadoutCategory Category);

	//その枠で今選んでいるもの(GetSelectableItems の中の 1 つ。無ければ nullptr)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	UObject* GetSelectedItem(ESavaLoadoutCategory Category);

	//枠の表示名(キャラクター・メイン武器など)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	static FText GetCategoryDisplayName(ESavaLoadoutCategory Category);

	//武器の枠なら true と、その武器の枠を返す(カスタマイズのボタンを出すかどうかに使う)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	static bool GetCategoryWeaponSlot(ESavaLoadoutCategory Category, ESavaWeaponSlot& Slot);

	//その武器を選んだときの数値(選んでいる武器なら今のカスタマイズ、そうでなければ前に決めたカスタマイズを適用する)
	//選択画面で、カーソルを乗せた武器と今の武器を比べる用
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	FSavaWeaponStats GetWeaponStatsWithSavedTuning(const USavaWeaponData* Weapon) const;

	//ロードアウトが変わった
	UPROPERTY(BlueprintAssignable, Category = "Sava|Loadout")
	FSavaOnLocalLoadoutChanged OnLoadoutChanged;

private:
	//選べないものを直して持ち、保存して知らせる
	void ApplyChange(const FSavaLoadout& NewLoadout);

	//今の武器のカスタマイズを、武器ごとの記録に入れる
	void RememberWeaponTunings();

	void LoadFromDisk();
	void SaveToDisk() const;

	FSavaLoadout Loadout;

	//武器ごとのカスタマイズ(武器を選び直したときに戻す用。ロードアウトと一緒に保存する)
	TMap<FSoftObjectPath, FSavaWeaponTuning> WeaponTunings;

	//一覧の項目(画面を開き直しても同じものを返す)
	UPROPERTY(Transient)
	TArray<TObjectPtr<USavaCharacterOption>> CharacterOptions;
};
