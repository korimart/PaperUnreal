#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransformAwaitableTest, "PaperUnreal.PaperUnreal.Test.TransformAwaitableTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTransformAwaitableTest::RunTest(const FString& Parameters)
{
	{
		auto [Promise, Future] = MakePromise<void>();

		int32 Received = 0;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			int32 Result = co_await (Future | Awaitables::Transform([](const auto&) { return 3; }));
			Received = Result;
		});

		TestEqual(TEXT("Transform 함수 테스트"), Received, 0);
		Promise.SetValue();
		TestEqual(TEXT("Transform 함수 테스트"), Received, 3);
	}

	{
		TLiveData<int32> LiveData;

		TArray<int32> Received;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			auto TimesTwoStream = LiveData.CreateStream()
				| Awaitables::AbortIfError()
				| Awaitables::Transform([](int32 Value) { return Value * 2; });

			while (true)
			{
				Received.Add(co_await TimesTwoStream);
			}
		});

		LiveData = 1;
		LiveData = 2;
		LiveData = 3;
		LiveData = 4;
		LiveData = 5;

		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received.Num(), 6);
		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received[0], 0);
		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received[1], 2);
		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received[2], 4);
		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received[3], 6);
		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received[4], 8);
		TestEqual(TEXT("Transform으로 만든 Awaitable이 재사용 가능한지 테스트"), Received[5], 10);
	}

	return true;
}
