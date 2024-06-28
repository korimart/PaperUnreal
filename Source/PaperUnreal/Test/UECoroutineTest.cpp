#include "UECoroutineTest.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(UECoroutineTest, "PaperUnreal.PaperUnreal.Test.UECoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

#define RETURN_IF_FALSE(boolean) if (!boolean) return true;

bool UECoroutineTest::RunTest(const FString& Parameters)
{
	UUECoroutineTestValueProvider* Provider = NewObject<UUECoroutineTestValueProvider>();

	{
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<int32> Reached;

		RunWeakCoroutine(Temp, [&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await CreateReadyWeakAwaitable(0));
			Reached.Add(co_await CreateReadyWeakAwaitable(1));
			Reached.Add(co_await CreateReadyWeakAwaitable(2));
			Reached.Add(co_await CreateReadyWeakAwaitable(3));
			Reached.Add(co_await CreateReadyWeakAwaitable(4));
		});
		
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 5));
	}

	{
		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<int32> Reached;

		RunWeakCoroutine(Temp, [Provider, &Reached, Lifetime = MoveTemp(Lifetime)](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
		});

		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 0));
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));
		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 1));
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));
		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));

		Temp->MarkAsGarbage();
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));

		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
	}

	{
		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<int32> Reached;

		RunWeakCoroutine(Temp, [Provider, &Reached, Lifetime = MoveTemp(Lifetime)](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
		});

		Provider->IssueValue(0);
		Provider->IssueValue(0);
		Provider->ClearRequests();
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
	}

	{
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		
		DECLARE_MULTICAST_DELEGATE_OneParam(FInt32MulticastDelegate, int32);
		FInt32MulticastDelegate MulticastDelegate;

		const auto CreateAwaitable = [&]()
		{
			FWeakAwaitableInt32 Ret;
			Ret.SetValueFromMulticastDelegate(Temp, MulticastDelegate);
			return Ret;
		};
		
		TArray<int32> Reached;
		RunWeakCoroutine(Temp, [&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
		});
		
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 0));
		MulticastDelegate.Broadcast(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 1));
		MulticastDelegate.Broadcast(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		MulticastDelegate.Broadcast(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 3));
		MulticastDelegate.Broadcast(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 3));

		Reached.Empty();
		RunWeakCoroutine(Temp, [&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
		});
		
		MulticastDelegate.Broadcast(0);
		MulticastDelegate.Broadcast(0);
		MulticastDelegate.Broadcast(0);
		Temp->MarkAsGarbage();
		MulticastDelegate.Broadcast(0);
		MulticastDelegate.Broadcast(0);
		MulticastDelegate.Broadcast(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 3));
	}

	{
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;
		
		DECLARE_MULTICAST_DELEGATE_OneParam(FInt32MulticastDelegate, int32);
		auto MulticastDelegate = MakeUnique<FInt32MulticastDelegate>();

		const auto CreateAwaitable = [&]()
		{
			FWeakAwaitableInt32 Ret;
			Ret.SetValueFromMulticastDelegate(Temp, *MulticastDelegate);
			return Ret;
		};
		
		TArray<int32> Reached;
		RunWeakCoroutine(Temp, [&CreateAwaitable, &Reached, LifeTime = MoveTemp(Lifetime)](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
			Reached.Add(co_await CreateAwaitable());
		});
		
		MulticastDelegate->Broadcast(0);
		MulticastDelegate->Broadcast(0);
		MulticastDelegate->Broadcast(0);
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));
		MulticastDelegate = nullptr;
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 3));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
	}

	{
		UUECoroutineTestValueProvider* Temp0 = NewObject<UUECoroutineTestValueProvider>();
		UUECoroutineTestValueProvider* Temp1 = NewObject<UUECoroutineTestValueProvider>();
		UUECoroutineTestValueProvider* Temp2 = NewObject<UUECoroutineTestValueProvider>();
		UUECoroutineTestValueProvider* Temp3 = NewObject<UUECoroutineTestValueProvider>();
		UUECoroutineTestValueProvider* Temp4 = NewObject<UUECoroutineTestValueProvider>();
		
		TArray<UObject*> Reached;

		RunWeakCoroutine(Provider, [&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Reached.Add(co_await Provider->FetchObject());
			Reached.Add(co_await Provider->FetchObject());
			Reached.Add(co_await Provider->FetchObject());
			Reached.Add(co_await Provider->FetchObject());
			Reached.Add(co_await Provider->FetchObject());
		});
		
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 0));
		
		Provider->IssueValue(Temp0);
		Provider->IssueValue(Temp1);
		Provider->IssueValue(Temp2);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 3));
		
		Temp1->MarkAsGarbage();
		Provider->IssueValue(Temp3);
		Provider->IssueValue(Temp4);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 3));
	}
	
	return true;
}

#undef RETURN_IF_FALSE
