// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPointEventListener.h"
#include "TracerPathGenerator.h"
#include "TracerPointEventComponent.generated.h"


UCLASS()
class UTracerPointEventComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetEventSource(ITracerPathStream* Source)
	{
		check(!HasBeenInitialized());
		EventSource = Cast<UObject>(Source);
	}

	void AddEventListener(ITracerPointEventListener* Listener)
	{
		check(HasBeenInitialized());
		InitiateEventListeningSequence(Listener);
	}

private:
	UPROPERTY()
	TScriptInterface<ITracerPathStream> EventSource;

	UPROPERTY()
	TArray<TScriptInterface<ITracerPointEventListener>> EventListeners;

	FVector2D LastValidHead;

	UTracerPointEventComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(CastChecked<UActorComponent2>(EventSource.GetObject()));
	}

	void InitiateEventListeningSequence(ITracerPointEventListener* Listener)
	{
		RunWeakCoroutine(this, [this, Listener](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(Listener);
			
			while (true)
			{
				auto Stream = EventSource->GetRunningPathTail().CreateStrictAddStream();

				// 두 번째 점을 기다리는 동안에 Tail Stream이 종료되는 걸 방지하기 위해 두 번째 거 먼저 기다림
				// 두 번째 점이 있으면 첫 번째 점은 자동으로 있음
				const FTracerPathPoint2 SecondPoint = co_await EventSource->GetRunningPathHead();
				const TFailableResult<FTracerPathPoint2> FirstPoint = co_await WithError<UValueStreamError>(Stream);

				if (!FirstPoint)
				{
					continue;
				}

				Listener->OnTracerBegin();
				Listener->AddPoint(FirstPoint.GetResult().Point);
				Listener->AddPoint(SecondPoint.Point);
				
				bool bNextHeadIsNewPoint = false;

				// 여기의 &캡쳐는 Handle이 코루틴 프레임과 수명을 같이하고 &가 코루틴 프레임에 대한 캡쳐기 때문에 안전함
				FDelegateSPHandle Handle = EventSource->GetRunningPathHead().ObserveIfValid([&](const FTracerPathPoint2& Head)
				{
					bNextHeadIsNewPoint ? Listener->AddPoint(Head.Point) : Listener->SetPoint(-1, Head.Point);
					bNextHeadIsNewPoint = false;
				});

				while (TFailableResult<FTracerPathPoint2> Point = co_await WithError<UValueStreamError>(Stream))
				{
					check(!bNextHeadIsNewPoint);
					Listener->SetPoint(-1, Point.GetResult().Point);
					bNextHeadIsNewPoint = true;
				}

				Listener->OnTracerEnd();
			}
		});
	}
};
