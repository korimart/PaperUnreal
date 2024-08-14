// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameStateBase2.h"
#include "GameFramework/GameModeBase.h"
#include "ComponentGroupComponent.h"
#include "GameModeComponent.generated.h"


UCLASS(Within=GameModeBase)
class UGameModeComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	AGameStateBase2* GetGameState() const
	{
		return GetOuterAGameModeBase()->GetGameState<AGameStateBase2>();
	}

	TSubclassOf<APawn> GetDefaultPawnClass() const
	{
		return GetOuterAGameModeBase()->DefaultPawnClass;
	}

protected:
	UGameModeComponent()
	{
		SetIsReplicatedByDefault(false);
	}
	
	virtual void OnPostLogin(APlayerController* PC)
	{
	}

	virtual void OnPostLogout(APlayerController* PC)
	{
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (bHasParent)
		{
			return;
		}

		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			DispatchPostLogin(It->Get());
		}

		FGameModeEvents::OnGameModePostLoginEvent().AddWeakLambda(this, [this](auto, APlayerController* PC)
		{
			DispatchPostLogin(PC);
		});

		FGameModeEvents::OnGameModeLogoutEvent().AddWeakLambda(this, [this](auto, AController* PC)
		{
			DispatchPostLogout(CastChecked<APlayerController>(PC));
		});
	}

private:
	bool bHasParent = false;

	UPROPERTY()
	TArray<UGameModeComponent*> ChildGameModeComponents;

	virtual void OnNewChildComponent(UActorComponent2* Component) override
	{
		if (Component->IsA<UGameModeComponent>())
		{
			ChildGameModeComponents.Add(Cast<UGameModeComponent>(Component));
			ChildGameModeComponents.Last()->bHasParent = true;
		}
	}

	void DispatchPostLogin(APlayerController* PC)
	{
		OnPostLogin(PC);

		ForEachChildGameModeComponent([&](ThisClass* Child)
		{
			Child->DispatchPostLogin(PC);
		});
	}

	void DispatchPostLogout(APlayerController* PC)
	{
		OnPostLogout(PC);

		ForEachChildGameModeComponent([&](ThisClass* Child)
		{
			Child->DispatchPostLogout(PC);
		});
	}

	void ForEachChildGameModeComponent(const auto& Func)
	{
		ChildGameModeComponents.RemoveAll([&](UActorComponent2* Each) { return !IsValid(Each); });
		for (ThisClass* Each : ChildGameModeComponents)
		{
			Func(Each);
		}
	}
};
