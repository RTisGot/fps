// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "savaCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class USavaSettingsMenuController;
class USavaCharacterMovementComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AsavaCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	//1人称カメラ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** ダッシュ（押している間） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SprintAction;

	/** しゃがみ（押している間） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> CrouchAction;

	///
	UPROPERTY(EditDefaultsOnly, Category = UI, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;//生成した設定画面Widgetへの参照を保持する変数

	UPROPERTY(Transient)
	TObjectPtr<USavaSettingsMenuController> SettingsMenuController;

	
	
public:
	AsavaCharacter(const FObjectInitializer& ObjectInitializer);

	//設定メニューを閉じる関数/
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettingsMenu();

	/** 移動コンポーネント（ダッシュ・スライディングを持つ独自クラス） */
	UFUNCTION(BlueprintPure, Category = Movement)
	USavaCharacterMovementComponent* GetSavaMovement() const;

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** ダッシュキーの押し／離し */
	void SprintPressed();
	void SprintReleased();

	/** しゃがみキーの押し／離し（押している間だけしゃがむ） */
	void CrouchPressed();
	void CrouchReleased();


	void ToggleSettingsMenu();


protected:
	// APawn interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// スライディング中はしゃがみ入力中でもジャンプできるようにする
	virtual bool CanJumpInternal_Implementation() const override;
	// しゃがみでカメラが一瞬で下がらないよう、ずれを記録して Tick で戻す
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

private:
	/** しゃがみ／立ち上がりでカメラを滑らかに動かすための一時的なずれ（見た目だけ） */
	float CrouchCameraOffset = 0.f;

	/** ずれを戻す速さ。大きいほど速く追従する */
	UPROPERTY(EditAnywhere, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float CrouchCameraInterpSpeed = 12.f;

	/** BeginPlay 時のカメラ位置。ここに CrouchCameraOffset を足して表示する */
	FVector DefaultCameraLocation = FVector::ZeroVector;

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

