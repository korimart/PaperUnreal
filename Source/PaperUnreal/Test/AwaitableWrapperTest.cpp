#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/ValueStream.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwaitableWrapperTest, "PaperUnreal.PaperUnreal.Test.AwaitableWrapperTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FAwaitableWrapperTest::RunTest(const FString& Parameters)
{
	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());

		int32 CompletedIndex = -1;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			CompletedIndex = co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
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
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			CompletedIndex = co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
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
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			CompletedIndex = co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
		});

		TestEqual(TEXT("AnyOf 테스트: 이미 완료 거를 기다리기 시작해도 잘 되는지 테스트"), CompletedIndex, 1);
	}

	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());

		bool bAborted = true;
		int32 CompletedIndex = -1;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			CompletedIndex = co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
			bAborted = false;
		});

		TestTrue(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), bAborted);
		TestEqual(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), CompletedIndex, -1);
		Array.Empty();
		TestTrue(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), bAborted);
		TestEqual(TEXT("AnyOf 테스트: 아무도 완료하지 않은 경우"), CompletedIndex, -1);
	}

	{
		TValueStream<int32> Stream;
		auto Receiver = Stream.GetReceiver();

		int32 Received = 0;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto Result = co_await Filter(Stream, [](int32 Value) { return Value > 3; });
			Received = Result;
		});

		Receiver.Pin()->ReceiveValue(1);
		TestEqual(TEXT("Filter 함수 테스트"), Received, 0);
		Receiver.Pin()->ReceiveValue(2);
		TestEqual(TEXT("Filter 함수 테스트"), Received, 0);
		Receiver.Pin()->ReceiveValue(3);
		TestEqual(TEXT("Filter 함수 테스트"), Received, 0);
		Receiver.Pin()->ReceiveValue(4);
		TestEqual(TEXT("Filter 함수 테스트"), Received, 4);
	}

	{
		auto [Promise, Future] = MakePromise<void>();

		int32 Received = 0;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			int32 Result = co_await Transform(MoveTemp(Future), [](const TFailableResult<std::monostate>&)
			{
				return 3;
			});
			Received = Result;
		});

		TestEqual(TEXT("Transform 함수 테스트"), Received, 0);
		Promise.SetValue();
		TestEqual(TEXT("Transform 함수 테스트"), Received, 3);
	}

	return true;
}
