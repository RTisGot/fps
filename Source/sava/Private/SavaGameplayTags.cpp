// Fill out your copyright notice in the Description page of Project Settings.

#include "SavaGameplayTags.h"

namespace SavaGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill, "InputTag.Skill", "スキルのボタン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Gadget, "InputTag.Gadget", "ガジェットのボタン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Fire, "InputTag.Weapon.Fire", "射撃ボタン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Aim, "InputTag.Weapon.Aim", "エイム(ADS)ボタン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Reload, "InputTag.Weapon.Reload", "リロードボタン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Swap, "InputTag.Weapon.Swap", "武器の持ち替えボタン");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Type_Skill, "Ability.Type.Skill", "スキル");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Type_Gadget, "Ability.Type.Gadget", "ガジェット");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Type_Weapon, "Ability.Type.Weapon", "武器");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Fire, "Ability.Weapon.Fire", "射撃");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Reload, "Ability.Weapon.Reload", "リロード");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "死亡中");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Aiming, "State.Weapon.Aiming", "ADS 中");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Reloading, "State.Weapon.Reloading", "リロード中");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "ダメージ量");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Healing, "SetByCaller.Healing", "回復量");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cooldown, "SetByCaller.Cooldown", "クールダウン秒数");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown, "Cooldown", "クールダウン中を表すタグの親(例: Cooldown.Skill.<能力名>)");
}
