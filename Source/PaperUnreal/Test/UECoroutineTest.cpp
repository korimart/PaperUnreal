#include "UECoroutineTest.h"
#include "Misc/AutomationTest.h"
#include "PaperUnreal/Core/Utils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(UECoroutineTest, "PaperUnreal.PaperUnreal.Test.UECoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

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

	{
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnInt32, int32);
		FOnInt32 OnInt32;
		TValueGenerator<int32> IntGen = CreateMulticastValueGenerator(TArray{1, 2, 3}, OnInt32);
		
		TArray<int32> Received;
		RunWeakCoroutine(Provider, [&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Received.Add(co_await IntGen.Next());
			Received.Add(co_await IntGen.Next());
			Received.Add(co_await IntGen.Next());

			while (true)
			{
				Received.Add(co_await IntGen.Next());
			}
		});

		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 3));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[0], 1));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[1], 2));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[2], 3));

		OnInt32.Broadcast(4);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 4));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[3], 4));
		
		OnInt32.Broadcast(5);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 5));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[4], 5));
	}
	
	{
		TValueGenerator<int32> IntGen;
		auto Receiver = IntGen.GetReceiver();

		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;

		TArray<int32> Received;
		RunWeakCoroutine(Provider, [&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			Received.Add(co_await IntGen.Next());
			Received.Add(co_await IntGen.Next());
			Received.Add(co_await IntGen.Next());

			while (co_await IntGen.NextIfNotEnd())
			{
				Received.Add(IntGen.Pop());
			}

			Received.Add(42);
		});

		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 0));
		Receiver.Pin()->AddValue(1);
		Receiver.Pin()->AddValue(2);
		Receiver.Pin()->AddValue(3);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 3));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[0], 1));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[1], 2));
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received[2], 3));

		Receiver.Pin()->AddValue(50);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 4));
		
		Receiver.Pin()->AddValue(50);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 5));
		
		Receiver.Pin()->AddValue(50);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Num(), 6));
		
		Receiver.Pin()->End();
		RETURN_IF_FALSE(TestEqual(TEXT(""), Received.Last(), 42));
	}
	
	return true;
}
