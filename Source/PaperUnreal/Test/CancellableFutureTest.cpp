#include "CancellableFutureTest.h"
#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCancellableFutureTest, "PaperUnreal.PaperUnreal.Test.CancellableFutureTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCancellableFutureTest::RunTest(const FString& Parameters)
{
	{
		TCancellablePromise<void> Promise;
		TCancellableFuture<void> Future = Promise.GetFuture();

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = Result.Succeeded();
		});

		TestFalse(TEXT("void 타입에 대해 값이 Future에서 잘 받아지는지 테스트"), bReceived);
		Promise.SetValue();
		TestTrue(TEXT("void 타입에 대해 값이 Future에서 잘 받아지는지 테스트"), bReceived);
	}

	{
		TCancellablePromise<void> Promise;
		TCancellableFuture<void> Future = Promise.GetFuture();

		Promise.SetValue();

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = Result.Succeeded();
		});

		TestTrue(TEXT("void 타입에 대해 값이 미리 준비되어 있는 경우에 Future에서 잘 받아지는지 테스트"), bReceived);
	}

	{
		auto Promise = MakeUnique<TCancellablePromise<void>>();
		TCancellableFuture<void> Future = Promise->GetFuture();

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = Cast<UCancellableFutureError>(Result.GetErrors()[0])->What == TEXT("PromiseNotFulfilled");
		});

		TestFalse(TEXT("void 타입에 대해 약속이 지켜지지 않은 경우 Future에서 잘 받아지는지 테스트"), bReceived);
		Promise = nullptr;
		TestTrue(TEXT("void 타입에 대해 약속이 지켜지지 않은 경우 Future에서 잘 받아지는지 테스트"), bReceived);
	}

	struct FPassThisAround
	{
		int32 Copied = 0;
		int32 Moved = 0;

		FPassThisAround() = default;

		FPassThisAround(const FPassThisAround& Other)
		{
			Copied = Other.Copied + 1;
			Moved = Other.Moved;
		}

		FPassThisAround& operator=(const FPassThisAround& Other)
		{
			Copied = Other.Copied + 1;
			Moved = Other.Moved;
			return *this;
		}

		FPassThisAround(FPassThisAround&& Other) noexcept
		{
			Copied = Other.Copied;
			Moved = Other.Moved + 1;
		}

		FPassThisAround& operator=(FPassThisAround&& Other) noexcept
		{
			Copied = Other.Copied;
			Moved = Other.Moved + 1;
			return *this;
		}
	};

	{
		TCancellablePromise<FPassThisAround> Promise;
		TCancellableFuture<FPassThisAround> Future = Promise.GetFuture();

		bool bReceived = false;
		int32 CopyCount;
		Future.Then([&](const auto& Result)
		{
			bReceived = true;
			CopyCount = Result.GetResult().Copied;
		});

		TestFalse(TEXT("non void 타입에 대해 Future에서 잘 받아지는지 테스트"), bReceived);
		Promise.SetValue(FPassThisAround{});
		TestTrue(TEXT("non void 타입에 대해 Future에서 잘 받아지는지 테스트"), bReceived);
		TestEqual(TEXT("non void 타입에 대해 Future에서 잘 받아지는지 테스트"), CopyCount, 0);
	}

	{
		TShareableCancellablePromise<FPassThisAround> Promise0;
		TShareableCancellablePromise<FPassThisAround> Promise1 = Promise0;
		TShareableCancellablePromise<FPassThisAround> Promise2 = Promise0;
		TCancellableFuture<FPassThisAround> Future = Promise0.GetFuture();

		int32 Count = 0;
		Future.Then([&](const auto& Result)
		{
			Count++;
		});

		TestEqual(TEXT("Shareable Promise를 통해서도 Future에서 잘 받아지는지 테스트"), Count, 0);
		Promise1.SetValue(FPassThisAround{});
		TestEqual(TEXT("Shareable Promise를 통해서도 Future에서 잘 받아지는지 테스트"), Count, 1);
		Promise0.SetValue(FPassThisAround{});
		Promise2.SetValue(FPassThisAround{});
		TestEqual(TEXT("Shareable Promise를 통해서도 Future에서 잘 받아지는지 테스트"), Count, 1);
	}

	{
		TArray<TShareableCancellablePromise<FPassThisAround>> PromiseArray;
		auto Promise = MakeUnique<TShareableCancellablePromise<FPassThisAround>>();
		PromiseArray.Add(*Promise);
		PromiseArray.Add(*Promise);
		PromiseArray.Add(*Promise);
		Promise = nullptr;
		TCancellableFuture<FPassThisAround> Future = PromiseArray[0].GetFuture();

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = Cast<UCancellableFutureError>(Result.GetErrors()[0])->What == TEXT("PromiseNotFulfilled");
		});

		TestFalse(TEXT("Shareable Promise가 약속을 지키지 않을 때 Future에서 잘 받아지는지 테스트"), bReceived);
		PromiseArray.RemoveAt(0);
		TestFalse(TEXT("Shareable Promise가 약속을 지키지 않을 때 Future에서 잘 받아지는지 테스트"), bReceived);
		PromiseArray.RemoveAt(1);
		TestFalse(TEXT("Shareable Promise가 약속을 지키지 않을 때 Future에서 잘 받아지는지 테스트"), bReceived);
		PromiseArray.RemoveAt(0);
		TestTrue(TEXT("Shareable Promise가 약속을 지키지 않을 때 Future에서 잘 받아지는지 테스트"), bReceived);
	}

	{
		FSimpleMulticastDelegate MulticastDelegate;

		int32 Count = 0;
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const auto& Result)
		{
			if (Result.Succeeded())
			{
				Count++;
			}
		});

		TestEqual(TEXT("FSimpleMulticastDelegate에서 Future 만들기 테스트"), Count, 0);
		MulticastDelegate.Broadcast();
		TestEqual(TEXT("FSimpleMulticastDelegate에서 Future 만들기 테스트"), Count, 1);
		MulticastDelegate.Broadcast();
		TestEqual(TEXT("FSimpleMulticastDelegate에서 Future 만들기 테스트"), Count, 1);
		MulticastDelegate.Broadcast();
		MulticastDelegate.Broadcast();
		MulticastDelegate.Broadcast();
		TestEqual(TEXT("FSimpleMulticastDelegate에서 Future 만들기 테스트"), Count, 1);
	}

	{
		DECLARE_MULTICAST_DELEGATE_OneParam(FIntMulticastDelegate, int32);
		FIntMulticastDelegate MulticastDelegate;

		int32 Sum = 0;
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const auto& Result)
		{
			Sum += Result.GetResult();
		});

		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 0);
		MulticastDelegate.Broadcast(42);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 42);
		MulticastDelegate.Broadcast(99);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 42);
		MulticastDelegate.Broadcast(99);
		MulticastDelegate.Broadcast(99);
		MulticastDelegate.Broadcast(99);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 42);
	}

	{
		DECLARE_MULTICAST_DELEGATE_OneParam(FIntMulticastDelegate, const int32&);
		FIntMulticastDelegate MulticastDelegate;

		int32 Received = 0;
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const auto& Result)
		{
			Received = Result.GetResult();
		});

		TestEqual(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Received, 0);
		MulticastDelegate.Broadcast(42);
		TestEqual(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Received, 42);
	}
	
	{
		UDummy* Dummy = NewObject<UDummy>();

		auto [Promise, Future] = MakePromise<UDummy*>();
		Promise.SetValue(Dummy);

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = IsValid(Result.GetResult());
		});

		TestTrue(TEXT("UObject 타입에 대해 값이 Future에서 잘 받아지는지 테스트"), bReceived);
	}

	{
		UDummy* Dummy = NewObject<UDummy>();

		auto [Promise, Future] = MakePromise<UDummy*>();
		Promise.SetValue(Dummy);
		Dummy->MarkAsGarbage();

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = Result.GetErrors()[0]->template IsA<UInvalidObjectError>();
		});

		TestTrue(TEXT("Then 호출 전에 파괴된 UObject가 Then에서 dangling pointer가 되지 않고 적절한 에러를 반환하는지 테스트"), bReceived);
	}
	
	{
		UDummy* Dummy = NewObject<UDummy>();

		auto [Promise, Future] = MakePromise<TScriptInterface<UDummy>>();
		Promise.SetValue(Dummy);
		Dummy->MarkAsGarbage();

		bool bReceived = false;
		Future.Then([&](const auto& Result)
		{
			bReceived = Result.GetErrors()[0]->template IsA<UInvalidObjectError>();
		});

		TestTrue(TEXT("Then 호출 전에 파괴된 UObject Wrapper의 UObject가 Then에서 dangling pointer가 되지 않고 적절한 에러를 반환하는지 테스트"), bReceived);
	}

	return true;
};
