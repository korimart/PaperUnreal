// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Character2.h"
#include "PaperUnrealCharacter.generated.h"

UCLASS(Blueprintable)
class APaperUnrealCharacter : public ACharacter2
{
	GENERATED_BODY()

public:
	UPROPERTY()
	class UTracerMeshComponent* ClientTracerMesh;
		
	APaperUnrealCharacter();

	/** Returns TopDownCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	virtual void AttachServerMachineComponents() override;
	virtual void AttachPlayerMachineComponents() override;
};