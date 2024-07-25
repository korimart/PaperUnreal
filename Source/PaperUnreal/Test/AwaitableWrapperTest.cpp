#include "CancellableFutureTest.h"
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

		bool bOver = false;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
			bOver = true;
		});

		TestFalse(TEXT("AnyOf 테스트: 두 개 중에 앞에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), bOver);
		Array[0].Get<0>().SetValue();
		TestTrue(TEXT("AnyOf 테스트: 두 개 중에 앞에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), bOver);
	}

	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());

		bool bOver = false;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
			bOver = true;
		});

		TestFalse(TEXT("AnyOf 테스트: 두 개 중에 뒤에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), bOver);
		Array[1].Get<0>().SetValue();
		TestTrue(TEXT("AnyOf 테스트: 두 개 중에 뒤에 거가 먼저 완료되면 AnyOf도 완료하는지 테스트"), bOver);
	}

	{
		TArray<TTuple<TCancellablePromise<void>, TCancellableFuture<void>>> Array;
		Array.Add(MakePromise<void>());
		Array.Add(MakePromise<void>());
		Array[1].Get<0>().SetValue();

		bool bOver = false;
		RunWeakCoroutine([&](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await AnyOf(MoveTemp(Array[0].Get<1>()), MoveTemp(Array[1].Get<1>()));
			bOver = true;
		});

		TestTrue(TEXT("AnyOf 테스트: 이미 완료 거를 기다리기 시작해도 잘 되는지 테스트"), bOver);
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
			co_await MoveTemp(Ret);
			bAborted = false;
		});

		TestTrue(TEXT("이미 에러가 발생해 있는 걸 기다릴 때 Abort 하는지"), bAborted);
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

	return true;
}
