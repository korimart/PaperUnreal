// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/ValueStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "ComponentRegistry.generated.h"

/**
 * 
 */
UCLASS()
class UComponentRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FComponentEvent, UActorComponent*);

	FComponentEvent& GetComponentMulticastDelegate(UClass* Class, AActor* Owner) const
	{
		return MulticastDelegateMap.FindOrAdd({Class, Owner});
	}

	void OnComponentBeginPlay(UActorComponent* Component) const
	{
		if (FComponentEvent* Found = MulticastDelegateMap.Find({Component->GetClass(), Component->GetOwner()}))
		{
			Found->Broadcast(Component);
		}
	}

	void OnComponentEndPlay(UActorComponent* Component) const
	{
		if (FComponentEvent* Found = MulticastDelegateMap.Find({Component->GetClass(), Component->GetOwner()}))
		{
			// Component Stream은 해당 클래스의 컴포넌트 중에서 가장 최신의 것을 뱉는 Stream이므로
			// Stream이 nullptr를 받는 경우는 해당 클래스의 컴포넌트가 하나도 없을 때 뿐임
			// 하나라도 있으면 이벤트를 발생시키지 않는다
			TArray<UActorComponent*> Components;
			Component->GetOwner()->GetComponents(Component->GetClass(), Components);
			for (UActorComponent* Each : Components)
			{
				if (Each->HasBegunPlay())
				{
					return;
				}
			}

			Found->Broadcast(nullptr);
		}
	}

private:
	struct FOwnerAndComponentClass
	{
		UClass* ComponentClass;
		AActor* Owner;

		friend bool operator==(const FOwnerAndComponentClass& Left, const FOwnerAndComponentClass& Right)
		{
			return Left.Owner == Right.Owner && Left.ComponentClass == Right.ComponentClass;
		}

		friend uint32 GetTypeHash(const FOwnerAndComponentClass& OfThis)
		{
			// 적절히 좋은 해쉬 생성에 대한 ChatGPT4o의 추천
			return GetTypeHash(OfThis.ComponentClass) ^ (GetTypeHash(OfThis.Owner) << 1);
		}
	};

	mutable TMap<FOwnerAndComponentClass, FComponentEvent> MulticastDelegateMap;
};


template <typename ComponentType>
// 현재 UActorComponentEx만 Registry를 사용하므로 실수 다른 컴포넌트를 넣지 않도록 체크
	requires std::is_base_of_v<UActorComponent2, ComponentType>
TCancellableFuture<ComponentType*> WaitForComponent(AActor* Owner)
{
	if (!IsValid(Owner))
	{
		check(false);
		return nullptr;
	}

	if (ComponentType* Found = ValidOrNull(Owner->FindComponentByClass<ComponentType>()))
	{
		if (Found->HasBegunPlay())
		{
			return Found;
		}
	}

	UComponentRegistry* Registry = Owner->GetWorld()->GetSubsystem<UComponentRegistry>();
	return MakeFutureFromDelegate(
		Registry->GetComponentMulticastDelegate(ComponentType::StaticClass(), Owner),
		[](UActorComponent* Component) { return IsValid(Component); },
		[](UActorComponent* BeforeCast) { return Cast<ComponentType>(BeforeCast); });
}


template <typename ComponentType>
// 현재 UActorComponentEx만 Registry를 사용하므로 실수 다른 컴포넌트를 넣지 않도록 체크
	requires std::is_base_of_v<UActorComponent2, ComponentType>
TValueStream<ComponentType*> MakeComponentStream(AActor* Owner)
{
	if (!IsValid(Owner))
	{
		check(false);
		return {};
	}

	// TODO 같은 클래스의 컴포넌트가 여러 개 존재하는 경우 stream에 컴포넌트의 생성 순서대로 들어감이
	TArray<ComponentType*> Initial;
	Owner->GetComponents<ComponentType>(Initial);
	Initial.RemoveAll([](ComponentType* Each) { return !Each->HasBegunPlay(); });

	UComponentRegistry* Registry = Owner->GetWorld()->GetSubsystem<UComponentRegistry>();
	
	auto Ret = MakeStreamFromDelegate(
		Registry->GetComponentMulticastDelegate(ComponentType::StaticClass(), Owner),
		[](auto) { return true; },
		[](UActorComponent* BeforeCast) { return Cast<ComponentType>(BeforeCast); });
	Ret.GetReceiver().Pin()->ReceiveValues(Initial);
	return Ret;
}
