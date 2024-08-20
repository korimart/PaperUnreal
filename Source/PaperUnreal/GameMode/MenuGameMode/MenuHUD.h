// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "PaperUnreal/PaperUnrealGameInstance.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/Widgets/LoadingWidgetSubsystem.h"
#include "PaperUnreal/Widgets/MenuWidget.h"
#include "PaperUnreal/Widgets/ToastWidget.h"
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

	UPROPERTY(EditAnywhere)
	TSubclassOf<UToastWidget> ToastWidgetClass;

	UPROPERTY()
	UMenuWidget* MenuWidget;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		SetupLevelBackground();

		GetOwningPlayerController()->SetShowMouseCursor(true);

		GetWorld()->GetSubsystem<ULoadingWidgetSubsystem>()->Remove();
		MenuWidget = CreateWidget<UMenuWidget>(GetOwningPlayerController(), MenuWidgetClass);
		MenuWidget->AddToViewport();

		DisplayNetworkErrorIfError();

		ListenToHostRequest();
		ListenToJoinRequest();
		ListenToQuitRequest();
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

	FWeakCoroutine DisplayNetworkErrorIfError()
	{
		UPaperUnrealGameInstance* GameInstance = GetWorld()->GetGameInstance<UPaperUnrealGameInstance>();

		const bool bReturnedToMenuByError = UGameplayStatics::HasOption(GetWorld()->GetAuthGameMode()->OptionsString, TEXT("closed"));
		const TOptional<ENetworkFailure::Type> Error = GameInstance->GetLastNetworkFailure();
		GameInstance->ClearErrors();

		if (bReturnedToMenuByError && !Error)
		{
			// TODO 엔진을 다 안 읽어봐서 어떤 경우에 이게 발생할 수 있는지 모름 그러므로 로그를 남긴다 log error
			co_return;
		}

		if (bReturnedToMenuByError && Error)
		{
			UToastWidget* Toast = CreateWidget<UToastWidget>(GetOwningPlayerController(), ToastWidgetClass);
			auto S = ScopedAddToViewport(Toast);

			FText ErrorMessage;
			if (*Error == ENetworkFailure::ConnectionLost)
			{
				ErrorMessage = FText::FromString(TEXT("방장이 게임을 나가서 방이 닫혔습니다."));
			}
			else
			{
				ErrorMessage = FText::FromString(TEXT("알 수 없는 네트워크 에러가 발생했습니다."));
			}

			auto H = Toast->Toast(ErrorMessage);
			co_await WaitForSeconds(GetWorld(), 5.f);
		}
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
			auto Failure = MakeFutureFromDelegate(GEngine->OnTravelFailure());
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

	FWeakCoroutine ListenToQuitRequest()
	{
		co_await MenuWidget->OnQuitPressed;
		UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayerController(), EQuitPreference::Quit, false);
	}
};
