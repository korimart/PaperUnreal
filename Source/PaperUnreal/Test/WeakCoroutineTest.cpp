#include "CancellableFutureTest.h"
#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeakCoroutineTest, "PaperUnreal.PaperUnreal.Test.WeakCoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

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
		RunWeakCoroutine([&Array, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			co_await Array[0].Get<1>();
			Received.Add(0);
			co_await Array[1].Get<1>();
			Received.Add(0);
			co_await Array[2].Get<1>();
			Received.Add(0);
			co_await Array[3].Get<1>();
			Received.Add(0);
			co_await Array[4].Get<1>();
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
		RunWeakCoroutine([&Array, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Received.Add(co_await Array[0].Get<1>());
			Received.Add(co_await Array[1].Get<1>());
			Received.Add(co_await Array[2].Get<1>());
			Received.Add(co_await Array[3].Get<1>());
			Received.Add(co_await Array[4].Get<1>());
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
		auto AbortIfNotValid = NewObject<UDummy>();

		TArray<int32> Received;
		RunWeakCoroutine([&Array, &AbortIfNotValid, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(AbortIfNotValid);

			Received.Add(co_await Array[0].Get<1>());
			Received.Add(co_await Array[1].Get<1>());
			Received.Add(co_await Array[2].Get<1>());
			Received.Add(co_await Array[3].Get<1>());
			Received.Add(co_await Array[4].Get<1>());
		});

		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received.Num(), 0);
		TestFalse(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		Array[0].Get<0>().SetValue(0);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received[0], 0);
		TestFalse(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);
		Array[1].Get<0>().SetValue(1);
		TestEqual(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), Received[1], 1);
		TestFalse(TEXT("AbortIfNotValid가 잘 작동하는는지 테스트"), *bLifeDestroyed);

		AbortIfNotValid->MarkAsGarbage();
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
		RunWeakCoroutine([&Array, &Received, Life = MoveTemp(Life)](FWeakCoroutineContext& Context) -> FWeakCoroutine
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
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());

		TArray<TArray<UFailableResultError*>> Received;
		RunWeakCoroutine([&](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Received.Add((co_await WithError(MoveTemp(Array[4].Get<1>()))).GetErrors());
			Received.Add((co_await WithError<UDummyError, UCancellableFutureError>(MoveTemp(Array[3].Get<1>()))).GetErrors());
			Received.Add((co_await WithError<UDummyError, UCancellableFutureError, UDummyError2>(MoveTemp(Array[2].Get<1>()))).GetErrors());
			Received.Add((co_await WithError<UDummyError2>(MoveTemp(Array[1].Get<1>()))).GetErrors());
			Received.Add((co_await WithError(MoveTemp(Array[0].Get<1>()))).GetErrors());
		});

		TestEqual(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), Received.Num(), 0);
		Array.RemoveAt(4);
		TestEqual(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), Received.Num(), 1);
		Array.RemoveAt(3);
		TestEqual(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), Received.Num(), 2);
		Array.RemoveAt(2);
		TestEqual(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), Received.Num(), 3);
		Array.RemoveAt(1);
		TestEqual(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), Received.Num(), 3);
		Array.RemoveAt(0);
		TestEqual(TEXT("WithError 함수로 Error를 받을 수 있는지 테스트"), Received.Num(), 3);
	}

	{
		TArray<TTuple<TCancellablePromise<UDummy*>, TCancellableFuture<UDummy*>>> Array;
		Array.Add(MakePromise<UDummy*>());
		Array.Add(MakePromise<UDummy*>());

		TArray<TArray<UFailableResultError*>> Received;
		RunWeakCoroutine([&](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			TFailableResult<TAbortPtr<UDummy>> ReceivedDummy = co_await WithError(MoveTemp(Array[0].Get<1>()));
			Received.Add((co_await WithError(MoveTemp(Array[1].Get<1>()))).GetErrors());
		});
		
		TestEqual(TEXT("WithError 함수가 UWeakCoroutineError만큼은 절대로 통과 안 시키는지 체크"), Received.Num(), 0);
		UDummy* Dummy = NewObject<UDummy>();
		Array[0].Get<0>().SetValue(Dummy);
		Dummy->MarkAsGarbage();
		TestEqual(TEXT("WithError 함수가 UWeakCoroutineError만큼은 절대로 통과 안 시키는지 체크"), Received.Num(), 0);
		Array.Empty();
		TestEqual(TEXT("WithError 함수가 UWeakCoroutineError만큼은 절대로 통과 안 시키는지 체크"), Received.Num(), 0);
	}

	{
		int32 Received = 0;
		RunWeakCoroutine([&](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Received = co_await RunWeakCoroutine([](TWeakCoroutineContext<int32>&) -> TWeakCoroutine<int32>
			{
				co_return 42;
			});
		});

		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 42);
	}

	{
		TArray<TTuple<TCancellablePromise<int32>, TCancellableFuture<int32>>> Array;
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());

		int32 Received = 0;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto Int0 = RunWeakCoroutine([&](TWeakCoroutineContext<int32>&) -> TWeakCoroutine<int32>
			{
				co_return co_await Array[0].Get<1>();
			});

			auto Int1 = RunWeakCoroutine([&](TWeakCoroutineContext<int32>&) -> TWeakCoroutine<int32>
			{
				int32 Sum = 0;
				Sum += co_await Array[1].Get<1>();
				co_return Sum + co_await Array[2].Get<1>();
			});

			auto Int2 = RunWeakCoroutine([&](TWeakCoroutineContext<int32>&) -> TWeakCoroutine<int32>
			{
				int32 Sum = 0;
				Sum += co_await Array[4].Get<1>();
				co_return Sum + co_await Array[3].Get<1>();
			});

			Received += co_await Int1;
			Received += co_await Int2;
			Received += co_await Int0;
		});

		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 0);
		Array[2].Get<0>().SetValue(10);
		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 0);
		Array[1].Get<0>().SetValue(20);
		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 30);
		Array[0].Get<0>().SetValue(30);
		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 30);
		Array[4].Get<0>().SetValue(40);
		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 30);
		Array[3].Get<0>().SetValue(50);
		TestEqual(TEXT("값을 반환하는 TWeakCoroutine 테스트"), Received, 150);
	}

	{
		UDummy* Dummy = NewObject<UDummy>();

		FStreamableDelegate Delegate;
		auto Ret = MakeFutureFromDelegate<UObject*>(
			Delegate,
			[]() { return true; },
			[Dummy]() { return Dummy; });

		Delegate.Execute();
		Dummy->MarkAsGarbage();

		bool bAborted = true;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await Ret;
			bAborted = false;
		});

		TestTrue(TEXT("이미 에러가 발생해 있는 걸 기다릴 때 Abort 하는지"), bAborted);
	}

	{
		auto [Promise, Future] = MakePromise<UDummy*>();
		UDummy* Null = nullptr;
		Promise.SetValue(Null);

		bool bReceived = false;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto Dummy = co_await Future;
			bReceived = Dummy == nullptr;
		});

		TestTrue(TEXT("nullptr인 UObject를 잘 받을 수 있는지"), bReceived);
	}

	{
		auto [Promise, Future] = MakePromise<UDummy*>();
		UDummy* Dummy = NewObject<UDummy>();

		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			TAbortPtr<UDummy> Result = co_await Future;

			// 컴파일 되지 않아야 함
			// UDummy* Result = co_await Future;

			TestTrue(TEXT("UObject 타입이 TAbortPtr로 잘 Transform 되는지 테스트"), Result == Dummy);
		});

		Promise.SetValue(Dummy);
	}
	
	{
		auto [Promise, Future] = MakePromise<TWeakObjectPtr<UDummy>>();
		UDummy* Dummy = NewObject<UDummy>();

		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			TWeakObjectPtr<UDummy> Result = co_await Future;
			TestTrue(TEXT("Safe UObject Wrapper 타입은 TAbortPtr로 Transform 안 되어야 되는 것 테스트"), Result == Dummy);
		});

		Promise.SetValue(Dummy);
	}

	{
		auto [Promise, Future] = MakePromise<UDummy*>();
		UDummy* Dummy = NewObject<UDummy>();

		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			TFailableResult<TAbortPtr<UDummy>> Result = co_await WithError(Future);
			TestTrue(TEXT("WithError를 씌워도 TAbortPtr로 잘 Transform 되는지 테스트"), Result.GetResult() == Dummy);
		});

		Promise.SetValue(Dummy);
	}

	{
		TArray<TTuple<TCancellablePromise<UDummy*>, TCancellableFuture<UDummy*>>> Array;
		Array.Add(MakePromise<UDummy*>());
		Array.Add(MakePromise<UDummy*>());
		Array.Add(MakePromise<UDummy*>());
		Array.Add(MakePromise<UDummy*>());
		Array.Add(MakePromise<UDummy*>());

		TArray<UDummy*> Dummies
		{
			nullptr,
			NewObject<UDummy>(),
			NewObject<UDummy>(),
			NewObject<UDummy>(),
			NewObject<UDummy>(),
		};

		TArray<UDummy*> Received;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			{
				auto Result = co_await Array[0].Get<1>();
				Received.Add(Result);
			}

			{
				auto Result = co_await Array[1].Get<1>();
				Received.Add(Result);
			}

			{
				auto Result = co_await Array[2].Get<1>();
				Received.Add(Result);
			}

			{
				auto Result0 = co_await Array[3].Get<1>();
				Received.Add(Result0);
				auto Result1 = co_await Array[4].Get<1>();
				Received.Add(Result1);
			}
		});

		for (int32 i = 0; i < 5; i++)
		{
			Array[i].Get<0>().SetValue(Dummies[i]);

			if (Dummies[i])
			{
				Dummies[i]->MarkAsGarbage();
			}
		}

		TestEqual(TEXT("TAbortPtr가 Abort 상태를 잘 관리하는지 테스트"), Received.Num(), 4);
		TestEqual(TEXT("TAbortPtr가 Abort 상태를 잘 관리하는지 테스트"), Received[0], Dummies[0]);
		TestEqual(TEXT("TAbortPtr가 Abort 상태를 잘 관리하는지 테스트"), Received[1], Dummies[1]);
		TestEqual(TEXT("TAbortPtr가 Abort 상태를 잘 관리하는지 테스트"), Received[2], Dummies[2]);
		TestEqual(TEXT("TAbortPtr가 Abort 상태를 잘 관리하는지 테스트"), Received[3], Dummies[3]);
	}

	return true;
}
