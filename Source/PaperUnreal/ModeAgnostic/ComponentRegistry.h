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
		return Found;
	}

	UComponentRegistry* Registry = Owner->GetWorld()->GetSubsystem<UComponentRegistry>();
	return MakeFutureFromDelegate<ComponentType*>(
		Registry->GetComponentMulticastDelegate(ComponentType::StaticClass(), Owner),
		[](UActorComponent* Component) { return IsValid(Component); },
		[](UActorComponent* BeforeCast) { return Cast<ComponentType>(BeforeCast); });
}


// TODO 같은 타입의 컴포넌트가 여러개 들어오는 경우에 대해 대응되어 있지 않음
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

	TArray<ComponentType*> Initial;
	if (ComponentType* Found = ValidOrNull(Owner->FindComponentByClass<ComponentType>()))
	{
		Initial.Add(Found);
	}

	UComponentRegistry* Registry = Owner->GetWorld()->GetSubsystem<UComponentRegistry>();
	return MakeStreamFromDelegate<ComponentType*>(
			Registry->GetComponentMulticastDelegate(ComponentType::StaticClass(), Owner),
			Initial,
			[](auto) { return true; },
			[](UActorComponent* BeforeCast) { return Cast<ComponentType>(BeforeCast); })
		.template Get<0>();
}
