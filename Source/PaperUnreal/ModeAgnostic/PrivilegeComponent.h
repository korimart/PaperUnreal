// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PrivilegeComponent.generated.h"


UCLASS(Within=PlayerState)
class UPrivilegeComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(bHost);

	void AddHostComponentClass(UClass* Class)
	{
		check(GetNetMode() != NM_Client);
		HostComponentClasses.Add(Class);
		OnHostComponentsMaybeChanged();
	}

	void RemoveHostComponentClass(UClass* Class)
	{
		check(GetNetMode() != NM_Client);
		HostComponentClasses.Remove(Class);
		OnHostComponentsMaybeChanged();
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_bHost)
	bool RepbHost = false;
	mutable TLiveData<bool&> bHost{RepbHost};

	UFUNCTION()
	void OnRep_bHost() { bHost.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbHost);
	}

	UPROPERTY()
	TArray<UClass*> HostComponentClasses;

	UPrivilegeComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() != NM_Client)
		{
			GetbHost().Observe(this, [this](auto) { OnHostComponentsMaybeChanged(); });
		}
	}

	void OnHostComponentsMaybeChanged()
	{
		bHost.Get() ? AttachHostComponents() : RemoveHostComponents();
	}

	void AttachHostComponents()
	{
		APlayerController* PC = GetOuterAPlayerState()->GetPlayerController();
		for (UClass* Each : HostComponentClasses)
		{
			if (!HasComponentOfClass(PC, Each))
			{
				NewObject<UActorComponent>(PC, Each)->RegisterComponent();
			}
		}
	}

	void RemoveHostComponents()
	{
		APlayerController* PC = GetOuterAPlayerState()->GetPlayerController();
		for (UActorComponent* Each : PC->GetComponents())
		{
			if (IsHostComponent(Each))
			{
				Each->DestroyComponent();
			}
		}
	}

	bool IsHostComponent(UActorComponent* Component) const
	{
		for (UClass* Each : HostComponentClasses)
		{
			if (Component->IsA(Each))
			{
				return true;
			}
		}
		return false;
	}

	static bool HasComponentOfClass(AActor* Actor, UClass* Class)
	{
		for (UActorComponent* Each : Actor->GetComponents())
		{
			if (Each->IsA(Class))
			{
				return true;
			}
		}
		return false;
	}
};