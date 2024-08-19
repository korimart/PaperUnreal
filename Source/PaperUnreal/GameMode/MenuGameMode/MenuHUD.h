// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/Widgets/MenuWidget.h"
#include "MenuHUD.generated.h"

/**
 * 
 */
UCLASS()
class AMenuHUD : public AHUD2
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere)
	TSubclassOf<APawn> MenuPawnClass;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<UMenuWidget> MenuWidgetClass;
	
	UPROPERTY()
	UMenuWidget* MenuWidget;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		
		SetupLevelBackground();
		
		GetOwningPlayerController()->SetShowMouseCursor(true);
		
		MenuWidget = CreateWidget<UMenuWidget>(GetOwningPlayerController(), MenuWidgetClass);
		MenuWidget->AddToViewport();

		ListenToHostRequest();
		ListenToJoinRequest();
	}

	void SetupLevelBackground()
	{
		GetOwningSpectatorPawn2()->SetActorLocation({500.f, 500.f, 100.f});
		GetOwningSpectatorPawn2()->SetActorRotation({0.f, 45.f, 0.f});

		const FVector Location{300.f, 1500.f, 100.f};
		APawn* MenuPawn = GetWorld()->SpawnActor<APawn>(MenuPawnClass, Location, FRotator::ZeroRotator);

		TValueStream<FLinearColor> ColorStream;
		ColorStream.GetReceiver().Pin()->ReceiveValue(NonEyeSoaringRandomColor());
		
		UTracerComponent* Tracer = NewObject<UTracerComponent>(MenuPawn);
		Tracer->SetTracerColorStream(MoveTemp(ColorStream));
		Tracer->RegisterComponent();

		AAIController* AIController = GetWorld()->SpawnActor<AAIController>();
		AIController->Possess(MenuPawn);
		AIController->MoveToLocation({600.f, 700.f, 100.f});
	}

	FWeakCoroutine ListenToHostRequest()
	{
		co_await MenuWidget->OnHostPressed;
		UGameplayStatics::OpenLevel(GetWorld(), TEXT("TopDownMap"), true, TEXT("Game=PVPBattle?listen"));
	}
	
	FWeakCoroutine ListenToJoinRequest()
	{
		const FString HostAddress = co_await MenuWidget->OnJoinPressed;
	}
};
