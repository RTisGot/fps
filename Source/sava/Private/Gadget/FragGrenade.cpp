#include "Gadget/FragGrenade.h"

AFragGrenade::AFragGrenade()
{
}

void AFragGrenade::OnGadgetLanded(const FHitResult& Hit)
{
	// グレネードは着弾した瞬間に爆発する
	Detonate();
}

void AFragGrenade::ApplyGadgetEffect()
{
	// TODO:
	// 効果範囲内の対象に100ダメージを与える。
	// 効果範囲の具体的な数値は未定。
}