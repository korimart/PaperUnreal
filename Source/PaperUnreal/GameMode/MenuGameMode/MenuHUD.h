// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
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

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTravelFailture, ETravelFailure::Type);
	FOnTravelFailture OnTravelFailure;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GEngine->OnTravelFailure().AddWeakLambda(this,
			[this](UWorld* World, ETravelFailure::Type Type, const FString& ErrorString)
			{
				OnTravelFailure.Broadcast(Type);
			});

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
		while (true)
		{
			const FString HostAddress = co_await MenuWidget->OnJoinPressed;

			// OpenLevel을 호출하는 즉시 Delegate가 호출될 수 있으므로 미리 awaitable을 만들어 둠
			auto Failure = MakeFutureFromDelegate(OnTravelFailure);
			UGameplayStatics::OpenLevel(GetWorld(), *HostAddress);
			
			const int32 Reason = co_await Awaitables::AnyOf(Failure, MenuWidget->OnCancelJoinPressed);
			if (Reason == 0)
			{
				MenuWidget->OnJoinFailed();
			}
			else
			{
				GEngine->CancelAllPending();
				MenuWidget->OnJoinCancelled();
			}
		}
	}
};
