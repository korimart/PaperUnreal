#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnyOfAwaitableTest, "PaperUnreal.PaperUnreal.Test.AnyOfAwaitableTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAnyOfAwaitableTest::RunTest(const FString& Parameters)
{
	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());

		int32 CompletedIndex = -1;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			CompletedIndex = co_await Awaitables::AnyOf(Array[0].Get<1>(), Array[1].Get<1>());
		});

		TestEqual(TEXT("AnyOf 테스트: 두 개 중에 앞에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), CompletedIndex, -1);
		Array[0].Get<0>().SetValue();
		TestEqual(TEXT("AnyOf 테스트: 두 개 중에 앞에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), CompletedIndex, 0);
	}

	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());

		int32 CompletedIndex = -1;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			CompletedIndex = co_await Awaitables::AnyOf(Array[0].Get<1>(), Array[1].Get<1>());
		});

		TestEqual(TEXT("AnyOf 테스트: 두 개 중에 뒤에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), CompletedIndex, -1);
		Array[1].Get<0>().SetValue();
		TestEqual(TEXT("AnyOf 테스트: 두 개 중에 뒤에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), CompletedIndex, 1);
	}

	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());
		Array[1].Get<0>().SetValue();

		int32 CompletedIndex = -1;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			CompletedIndex = co_await Awaitables::AnyOf(Array[0].Get<1>(), Array[1].Get<1>());
		});

		TestEqual(TEXT("AnyOf 테스트: 이미 완료 거를 기다리기 시작해도 잘 되는지 테스트"), CompletedIndex, 1);
	}

	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());

		bool bAborted = true;
		int32 CompletedIndex = -1;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			CompletedIndex = co_await Awaitables::AnyOf(Array[0].Get<1>(), Array[1].Get<1>());
			bAborted = false;
		});

		TestTrue(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), bAborted);
		TestEqual(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), CompletedIndex, -1);
		Array.Empty();
		TestTrue(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), bAborted);
		TestEqual(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), CompletedIndex, -1);
	}

	{
		TLiveData<int32> LiveData0;
		TLiveData<int32> LiveData1;

		TArray<int32> Received;
		FWeakCoroutine Coroutine = RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			auto AnyOfAwaitable = Awaitables::AnyOf(LiveData0.CreateStream(), LiveData1.CreateStream());

			while (true)
			{
				Received.Add(co_await AnyOfAwaitable);
			}
		});

		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[0], 0);
		
		LiveData0 = 1;
		LiveData1 = 1;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[1], 0);

		LiveData0 = 2;
		LiveData1 = 2;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[2], 0);
		
		LiveData0 = 3;
		LiveData1 = 3;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[3], 0);

		LiveData1 = 4;
		LiveData0 = 4;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[4], 1);
		
		LiveData1 = 5;
		LiveData0 = 5;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[5], 1);
		
		LiveData0 = 6;
		LiveData1 = 6;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received[6], 0);
		
		Coroutine.Abort();
		LiveData0 = 42;
		LiveData1 = 42;
		TestEqual(TEXT("AnyOf 테스트: 여러번 기다릴 수 있는 awaitable에 대해 여러번 await 가능한지"), Received.Num(), 7);
	}

	return true;
}
