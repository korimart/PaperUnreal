#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/ValueStream.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwaitableWrapperTest, "PaperUnreal.PaperUnreal.Test.AwaitableWrapperTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwaitableWrapperTest::RunTest(const FString& Parameters)
{
	{
		TValueStream<int32> Stream;
		auto Receiver = Stream.GetReceiver();

		int32 Received = 0;
		RunWeakCoroutine([&]() -> FWeakCoroutine
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
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			int32 Result = co_await Transform(Future, [](const TFailableResult<std::monostate>&)
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
