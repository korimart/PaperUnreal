// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PrivilegeComponent.generated.h"


class FConditionalComponents
{
public:
	FConditionalComponents() = default;

	void SetComponentOwnerAndGroup(AActor* Actor, UComponentGroupComponent* Group)
	{
		ComponentOwner = Actor;
		ComponentGroup = Group;
		OnComponentsMaybeChanged();
	}

	void SetCondition(bool bInCondition)
	{
		bCondition = bInCondition;
		OnComponentsMaybeChanged();
	}

	void AddClass(UClass* Class)
	{
		Classes.Add(Class);
		OnComponentsMaybeChanged();
	}

	void RemoveClass(UClass* Class)
	{
		Classes.Remove(Class);
		OnComponentsMaybeChanged();
	}

private:
	TArray<UClass*> Classes;
	TWeakObjectPtr<AActor> ComponentOwner;
	TWeakObjectPtr<UComponentGroupComponent> ComponentGroup;
	bool bCondition = false;

	void OnComponentsMaybeChanged();
};


UCLASS(Within=PlayerState)
class UPrivilegeComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	void AddComponentForPrivilege(FName Privilege, UClass* Class)
	{
		FindOrAdd(Privilege).AddClass(Class);
	}

	void RemoveComponentForPrivilege(FName Privilege, UClass* Class)
	{
		FindOrAdd(Privilege).RemoveClass(Class);
	}

	void GivePrivilege(FName Privilege)
	{
		FindOrAdd(Privilege).SetCondition(true);
	}

	void TakePrivilge(FName Privilege)
	{
		FindOrAdd(Privilege).SetCondition(false);
	}

private:
	TMap<FName, FConditionalComponents> NameToConditionalComponentsMap;

	UPrivilegeComponent()
	{
		SetIsReplicatedByDefault(false);
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		check(GetNetMode() != NM_Client);
	}

	FConditionalComponents& FindOrAdd(FName Privilege)
	{
		if (NameToConditionalComponentsMap.Contains(Privilege))
		{
			return NameToConditionalComponentsMap[Privilege];
		}

		FConditionalComponents& Ret = NameToConditionalComponentsMap.Emplace(Privilege);
		Ret.SetComponentOwnerAndGroup(GetOuterAPlayerState()->GetPlayerController(), this);
		return Ret;
	}
};