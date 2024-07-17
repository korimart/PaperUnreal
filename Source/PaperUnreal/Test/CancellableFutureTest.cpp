#include "Misc/AutomationTest.h"
#include "PaperUnreal/Core/WeakCoroutine/CancellableFuture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCancellableFutureTest, "PaperUnreal.PaperUnreal.Test.CancellableFutureTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FCancellableFutureTest::RunTest(const FString& Parameters)
{
	{
		TCancellablePromise<void> Promise;
		TCancellableFuture<void> Future = Promise.GetFuture();

		bool bReceived = false;
		Future.Then([&](const TVariant<std::monostate, EDefaultFutureError>& Result)
		{
			bReceived = Result.IsType<std::monostate>();
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
		Future.Then([&](const TVariant<std::monostate, EDefaultFutureError>& Result)
		{
			bReceived = Result.IsType<std::monostate>();
		});

		TestTrue(TEXT("void 타입에 대해 값이 미리 준비되어 있는 경우에 Future에서 잘 받아지는지 테스트"), bReceived);
	}

	{
		auto Promise = MakeUnique<TCancellablePromise<void>>();
		TCancellableFuture<void> Future = Promise->GetFuture();

		bool bReceived = false;
		Future.Then([&](const TVariant<std::monostate, EDefaultFutureError>& Result)
		{
			bReceived = Result.Get<EDefaultFutureError>() == EDefaultFutureError::PromiseNotFulfilled;
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
		Future.Then([&](const TVariant<FPassThisAround, EDefaultFutureError>& Result)
		{
			bReceived = true;
			CopyCount = Result.Get<FPassThisAround>().Copied;
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
		Future.Then([&](const TVariant<FPassThisAround, EDefaultFutureError>& Result)
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
		Future.Then([&](const TVariant<FPassThisAround, EDefaultFutureError>& Result)
		{
			bReceived = Result.Get<EDefaultFutureError>() == EDefaultFutureError::PromiseNotFulfilled;
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
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const TVariant<std::monostate, EDefaultFutureError>& Result)
		{
			check(!Result.IsType<EDefaultFutureError>());
			Count++;
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
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const TVariant<int32, EDefaultFutureError>& Result)
		{
			Sum += Result.Get<int32>();
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
		DECLARE_MULTICAST_DELEGATE_TwoParams(FManyMulticastDelegate, int32, float);
		FManyMulticastDelegate MulticastDelegate;

		int32 Sum = 0;
		float FSum = 0.f;
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const TVariant<TTuple<int32, float>, EDefaultFutureError>& Result)
		{
			Sum += Result.Get<TTuple<int32, float>>().Get<0>();
			FSum += Result.Get<TTuple<int32, float>>().Get<1>();
		});

		TestEqual(TEXT("파라미터가 두 개인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 0);
		TestEqual(TEXT("파라미터가 두 개인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), FSum, 0.f);
		MulticastDelegate.Broadcast(42, 42.f);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 42);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), FSum, 42.f);
		MulticastDelegate.Broadcast(99, 99.f);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 42);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), FSum, 42.f);
		MulticastDelegate.Broadcast(99, 99.f);
		MulticastDelegate.Broadcast(99, 99.f);
		MulticastDelegate.Broadcast(99, 99.f);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Sum, 42);
		TestEqual(TEXT("파라미터가 하나인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), FSum, 42.f);
	}
	
	{
		DECLARE_MULTICAST_DELEGATE_TwoParams(FManyMulticastDelegate, const int32&, const float&);
		FManyMulticastDelegate MulticastDelegate;

		const int32* First = nullptr;
		const float* Second = nullptr;
		MakeFutureFromDelegate(MulticastDelegate).Then([&](const TVariant<TTuple<const int32&, const float&>, EDefaultFutureError>& Result)
		{
			First = &Result.Get<TTuple<const int32&, const float&>>().Get<0>();
			Second = &Result.Get<TTuple<const int32&, const float&>>().Get<1>();
		});

		int32 IntVal = 0;
		float FloatVal = 0.f;
		
		TestTrue(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), First == nullptr);
		TestTrue(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), Second == nullptr);
		MulticastDelegate.Broadcast(IntVal, FloatVal);
		TestEqual(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), *First, 0);
		TestEqual(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), *Second, 0.f);
		IntVal = 42;
		FloatVal = 42.f;
		TestEqual(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), *First, 42);
		TestEqual(TEXT("파라미터가 레퍼런스인 멀티캐스트 델리게이트에서 Future 만들기 테스트"), *Second, 42.f);
	}

	return true;
};
