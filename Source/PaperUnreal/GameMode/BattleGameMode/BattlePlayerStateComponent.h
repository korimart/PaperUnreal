// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterSetterComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/TeamComponent.h"
#include "BattlePlayerStateComponent.generated.h"


UCLASS(Within=PlayerState)
class UBattlePlayerStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTeamComponent* ServerTeamComponent;

	TLiveDataView<UInventoryComponent*&> GetInventoryComponent() const
	{
		return InventoryComponent;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_InventoryComponent)
	UInventoryComponent* RepInventoryComponent;
	mutable TLiveData<UInventoryComponent*&> InventoryComponent{RepInventoryComponent};

	UFUNCTION()
	void OnRep_InventoryComponent() { InventoryComponent.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);

		DOREPLIFETIME_CONDITION(ThisClass, RepInventoryComponent, COND_InitialOnly);
	}

	virtual void AttachServerMachineComponents() override
	{
		ServerTeamComponent = NewChildComponent<UTeamComponent>();
		ServerTeamComponent->RegisterComponent();

		InventoryComponent = NewChildComponent<UInventoryComponent>();
		// 만약 이전 게임모드에서 골라놓은 게 있으면 그걸 기본값으로 일단 사용한다
		if (auto Setter = GetOuterAPlayerState()->GetPlayerController()->FindComponentByClass<UCharacterSetterComponent>())
		{
			InventoryComponent->SetCharacterMesh(Setter->MostRecentChoice);
		}
		InventoryComponent->RegisterComponent();
	}
};
