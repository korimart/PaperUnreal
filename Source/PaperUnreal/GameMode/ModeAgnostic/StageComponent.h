// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "StageComponent.generated.h"


USTRUCT()
struct FReplicatedStageInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName Stage;

	UPROPERTY()
	float WorldStartTime = 0.f;
};


UCLASS()
class UStageComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(CurrentStage);

	TLiveDataView<TOptional<float>> GetStageWorldStartTime(FName StageName) const
	{
		return StageWorldStartTime.FindOrAdd(StageName);
	}

	void SetStageWorldStartTime(FName StageName, float WorldStartTime)
	{
		check(GetOwner()->HasAuthority());
		
		FReplicatedStageInfo* Info = Algo::FindBy(RepStageInfos, StageName, &FReplicatedStageInfo::Stage);
		if (!Info)
		{
			Info = &RepStageInfos.AddDefaulted_GetRef();
			Info->Stage = StageName;
		}

		Info->WorldStartTime = WorldStartTime;

		if (GetNetMode() != NM_DedicatedServer)
		{
			OnRep_StageInfos();
		}
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentStage)
	FName RepCurrentStage;
	mutable TLiveData<FName&> CurrentStage{RepCurrentStage};

	UPROPERTY(ReplicatedUsing=OnRep_StageInfos)
	TArray<FReplicatedStageInfo> RepStageInfos;
	mutable TMap<FName, TLiveData<TOptional<float>>> StageWorldStartTime;

	UFUNCTION()
	void OnRep_CurrentStage() { CurrentStage.Notify(); }
	
	UFUNCTION()
	void OnRep_StageInfos()
	{
		for (const FReplicatedStageInfo& Each : RepStageInfos)
		{
			StageWorldStartTime.FindOrAdd(Each.Stage) = Each.WorldStartTime;
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepCurrentStage);
		DOREPLIFETIME(ThisClass, RepStageInfos);
	}

	UStageComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};
