﻿#include "Misc/AutomationTest.h"
#include "PaperUnreal/Core/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/Core/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeakCoroutineTest, "PaperUnreal.PaperUnreal.Test.WeakCoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FWeakCoroutineTest::RunTest(const FString& Parameters)
{
	struct FLife
	{
		TSharedPtr<bool> bDestroyed = MakeShared<bool>(false);

		~FLife()
		{
			*bDestroyed = true;
		}
	};
	
	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());
		
		auto Life = MakeUnique<FLife>();
		auto bLifeDestroyed = Life->bDestroyed;

		TArray<int32> Received;
		RunWeakCoroutine2([&Array, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
		{
			co_await MoveTemp(Array[0].Get<1>());
			Received.Add(0);
			co_await MoveTemp(Array[1].Get<1>());
			Received.Add(0);
			co_await MoveTemp(Array[2].Get<1>());
			Received.Add(0);
			co_await MoveTemp(Array[3].Get<1>());
			Received.Add(0);
			co_await MoveTemp(Array[4].Get<1>());
			Received.Add(0);
		});

		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 0);
		TestFalse(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[0].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 1);
		TestFalse(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[1].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 2);
		TestFalse(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[2].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 3);
		TestFalse(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[3].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 4);
		TestFalse(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[4].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 5);
		TestTrue(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
	}
	
	{
		TArray<TTuple<TCancellablePromise<int32>, TCancellableFuture<int32>>> Array;
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		
		auto Life = MakeUnique<FLife>();
		auto bLifeDestroyed = Life->bDestroyed;

		TArray<int32> Received;
		RunWeakCoroutine2([&Array, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
		{
			Received.Add(co_await MoveTemp(Array[0].Get<1>()));
			Received.Add(co_await MoveTemp(Array[1].Get<1>()));
			Received.Add(co_await MoveTemp(Array[2].Get<1>()));
			Received.Add(co_await MoveTemp(Array[3].Get<1>()));
			Received.Add(co_await MoveTemp(Array[4].Get<1>()));
		});

		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 0);
		TestFalse(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[0].Get<0>().SetValue(0);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[0], 0);
		TestFalse(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[1].Get<0>().SetValue(1);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[1], 1);
		TestFalse(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[2].Get<0>().SetValue(2);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[2], 2);
		TestFalse(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[3].Get<0>().SetValue(3);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[3], 3);
		TestFalse(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
		Array[4].Get<0>().SetValue(4);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[4], 4);
		TestTrue(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), *bLifeDestroyed);
	}

	{
		TArray<TTuple<TCancellablePromise<int32>, TCancellableFuture<int32>>> Array;
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		
		auto Life = MakeUnique<FLife>();
		auto bLifeDestroyed = Life->bDestroyed;
		auto AbortIfNotValid = MakeShared<bool>().ToSharedPtr();

		TArray<int32> Received;
		RunWeakCoroutine2([&Array, &AbortIfNotValid, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
		{
			Context.AbortIfNotValid(AbortIfNotValid);
			
			Received.Add(co_await MoveTemp(Array[0].Get<1>()));
			Received.Add(co_await MoveTemp(Array[1].Get<1>()));
			Received.Add(co_await MoveTemp(Array[2].Get<1>()));
			Received.Add(co_await MoveTemp(Array[3].Get<1>()));
			Received.Add(co_await MoveTemp(Array[4].Get<1>()));
		});

		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received.Num(), 0);
		TestFalse(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		Array[0].Get<0>().SetValue(0);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received[0], 0);
		TestFalse(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		Array[1].Get<0>().SetValue(1);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received[1], 1);
		TestFalse(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		
		AbortIfNotValid = nullptr;
		Array[2].Get<0>().SetValue(2);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received.Num(), 2);
		TestTrue(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		Array[3].Get<0>().SetValue(3);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received.Num(), 2);
		TestTrue(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		Array[4].Get<0>().SetValue(4);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received.Num(), 2);
		TestTrue(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
	}

	{
		TArray<TTuple<TCancellablePromise<int32>, TCancellableFuture<int32>>> Array;
		Array.Add(MakePromise<int32>());
		
		auto Life = MakeUnique<FLife>();
		auto bLifeDestroyed = Life->bDestroyed;
		
		TArray<int32> Received;
		RunWeakCoroutine2([&Array, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
		{
			Received.Add(co_await MoveTemp(Array[0].Get<1>()));
		});

		TestEqual(TEXT("약속이 지켜지지 않으면 종료하는지 테스트"), Received.Num(), 0);
		TestFalse(TEXT("약속이 지켜지지 않으면 종료하는지 테스트"), *bLifeDestroyed);
		Array.Empty();
		TestEqual(TEXT("약속이 지켜지지 않으면 종료하는지 테스트"), Received.Num(), 0);
		TestTrue(TEXT("약속이 지켜지지 않으면 종료하는지 테스트"), *bLifeDestroyed);
	}
	
	{
		TArray<TTuple<TCancellablePromise<int32>, TCancellableFuture<int32>>> Array;
		Array.Add(MakePromise<int32>());
		
		bool bError = false;
		RunWeakCoroutine2([&Array, &bError](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
		{
			auto ResultOrError = co_await WithError(MoveTemp(Array[0].Get<1>()));
			bError = ResultOrError.Get<EDefaultFutureError>() == EDefaultFutureError::PromiseNotFulfilled;
		});

		TestFalse(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), bError);
		Array.Empty();
		TestTrue(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), bError);
	}
	
	return true;
}