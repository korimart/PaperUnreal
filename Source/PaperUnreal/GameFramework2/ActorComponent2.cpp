// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent2.h"

#include "PaperUnreal/GameMode/ModeAgnostic/ComponentRegistry.h"

void UActorComponent2::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UComponentRegistry>()->OnComponentBeginPlay(this);
	OnBeginPlay.Broadcast();
}

void UActorComponent2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetSubsystem<UComponentRegistry>()->OnComponentEndPlay(this);
	OnEndPlay.Broadcast();
}

void UActorComponent2::AddLifeDependency(UActorComponent2* Dependency)
{
	check(IsValid(Dependency));
	Dependency->OnEndPlay.AddWeakLambda(this, [this]()
	{
		DestroyComponent();
	});
}
