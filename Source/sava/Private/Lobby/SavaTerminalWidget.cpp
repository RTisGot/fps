// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaTerminalWidget.h"
#include "Lobby/SavaInteractTerminal.h"
#include "InputCoreTypes.h"

void USavaTerminalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//キー入力(Esc)を受け取れるようにする
	SetIsFocusable(true);
}

ASavaInteractTerminal* USavaTerminalWidget::GetTerminal() const
{
	return Terminal.Get();
}

void USavaTerminalWidget::SetTerminal(ASavaInteractTerminal* InTerminal)
{
	Terminal = InTerminal;
}

void USavaTerminalWidget::CloseTerminal()
{
	if (ASavaInteractTerminal* OwningTerminal = Terminal.Get())
	{
		OwningTerminal->CloseMenu();
	}
	else
	{
		RemoveFromParent();
	}
}

FReply USavaTerminalWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	//ここで受け取ると、設定画面(Esc)は開かない
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!HandleBack())
		{
			CloseTerminal();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool USavaTerminalWidget::HandleBack_Implementation()
{
	return false;
}
