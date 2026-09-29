// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

//C++ から使う GameplayTag の一覧。エディタのタグ一覧にも自動で表示される
//スキルごとのクールダウンタグ(Cooldown.Skill.<能力名> など)は各担当がエディタで追加する
namespace SavaGameplayTags
{
	//入力(どのボタンで発動するか)
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Gadget);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Fire);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Aim);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Reload);

	//能力の種類(「ガジェット使用中は武器を撃てない」などの指定に使う)
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Skill);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Gadget);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Weapon);

	//状態
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	//GameplayEffect に数値を渡すためのキー
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Healing);
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cooldown);

	//クールダウンタグの親
	SAVA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown);
}
