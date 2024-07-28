// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPointEventListener.h"
#include "TracerPathProvider.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "TracerPointEventComponent.generated.h"


UCLASS()
class UTracerPointEventComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetEventSource(ITracerPathProvider* Source)
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
	TScriptInterface<ITracerPathProvider> EventSource;

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
				const FVector2D SecondPoint = co_await EventSource->GetRunningPathHead();
				const TFailableResult<FVector2D> FirstPoint = co_await WithError<UEndOfStreamError>(Stream);

				if (!FirstPoint)
				{
					continue;
				}

				Listener->OnTracerBegin();
				Listener->AddPoint(FirstPoint.GetResult());
				Listener->AddPoint(SecondPoint);
				
				bool bLastPointIsHead = true;

				// 여기의 &캡쳐는 Handle이 코루틴 프레임과 수명을 같이하고 &가 코루틴 프레임에 대한 캡쳐기 때문에 안전함
				FDelegateSPHandle Handle = EventSource->GetRunningPathHead().ObserveIfValid([&](const FVector2D& Head)
				{
					bLastPointIsHead ? Listener->SetPoint(-1, Head) : Listener->AddPoint(Head);
					bLastPointIsHead = true;
				});

				while (TFailableResult<FVector2D> Point = co_await WithError<UEndOfStreamError>(Stream))
				{
					bLastPointIsHead ? Listener->SetPoint(-1, Point.GetResult()) : Listener->AddPoint(Point.GetResult());
					bLastPointIsHead = false;
				}

				Listener->OnTracerEnd();
			}
		});
	}
};
