// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "LoadingWidgetSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class ULoadingWidgetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Remove()
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
			Widget = nullptr;
		}
	}

private:
	UPROPERTY()
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY()
	UUserWidget* Widget;

	ULoadingWidgetSubsystem()
	{
		static ConstructorHelpers::FClassFinder<UUserWidget> WidgetClassFinder{TEXT("/Game/TopDown/Widgets/W_LoadingWidget")};
		if (WidgetClassFinder.Class != nullptr)
		{
			WidgetClass = WidgetClassFinder.Class;
		}
	}

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override
	{
		if (!Super::ShouldCreateSubsystem(Outer))
		{
			return false;
		}

		return Cast<UWorld>(Outer)->GetNetMode() != NM_DedicatedServer;
	}

	virtual void OnWorldBeginPlay(UWorld& InWorld) override
	{
		Widget = CreateWidget(&InWorld, WidgetClass);
		Widget->AddToViewport();
	}

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
};
