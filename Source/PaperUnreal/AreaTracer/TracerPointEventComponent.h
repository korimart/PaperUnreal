// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "TracerPointEventListener.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "TracerPointEventComponent.generated.h"


UCLASS()
class UTracerPointEventComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetEventSource(UTracerPathComponent* Source)
	{
		check(!HasBeenInitialized());
		EventSource = Source;
	}

	void AddEventListener(ITracerPointEventListener* Listener)
	{
		check(HasBeenInitialized());
		InitiateEventListeningSequence(Listener);
	}

private:
	UPROPERTY()
	UTracerPathComponent* EventSource;

	UTracerPointEventComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(EventSource);
	}

	FWeakCoroutine InitiateEventListeningSequence(ITracerPointEventListener* Listener)
	{
		co_await AddToWeakList(Listener);

		while (true)
		{
			auto Stream = EventSource->GetRunningPathTail().MakeStrictAddStream() | Awaitables::Catch<UEndOfStreamError>();

			const TFailableResult<FVector2D> FirstPoint = co_await Stream;
			if (!FirstPoint)
			{
				continue;
			}

			Listener->OnTracerBegin();
			Listener->AddPoint(FirstPoint.GetResult());

			bool bLastPointIsHead = false;

			// 여기의 &캡쳐는 Handle이 코루틴 프레임과 수명을 같이하고 &가 코루틴 프레임에 대한 캡쳐기 때문에 안전함
			FDelegateSPHandle Handle = EventSource->GetRunningPathHead().ObserveIfValid([&](const FVector2D& Head)
			{
				bLastPointIsHead ? Listener->SetPoint(-1, Head) : Listener->AddPoint(Head);
				bLastPointIsHead = true;
			});

			while (TFailableResult<FVector2D> Point = co_await Stream)
			{
				bLastPointIsHead ? Listener->SetPoint(-1, Point.GetResult()) : Listener->AddPoint(Point.GetResult());
				bLastPointIsHead = false;
			}

			Listener->OnTracerEnd();
		}
	}
};
