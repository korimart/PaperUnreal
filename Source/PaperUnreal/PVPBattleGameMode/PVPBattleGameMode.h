// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PVPBattleGameMode.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogPVPBattleGameMode, Log, All);


namespace PVPBattlePrivilege
{
	inline FName Host{TEXT("PVPBattlePrivilege_Host")};
	inline FName Normie{TEXT("PVPBattlePrivilege_Normie")};
}


namespace PVPBattleStage
{
	inline FName WaitingForStart{TEXT("PVPBattleStage_WaitingForConfig")};
	inline FName Playing{TEXT("PVPBattleStage_Playing")};
	inline FName Result{TEXT("PVPBattleStage_Result")};
}


UCLASS()
class APVPBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	bool StartGameIfConditionsMet();

private:
	FSimpleMulticastDelegate OnGameStartConditionsMet;
	
	APVPBattleGameMode();

	virtual void BeginPlay() override;
	
	class UPrivilegeComponent* AddPrivilegeComponents(UPrivilegeComponent* Target);
};
