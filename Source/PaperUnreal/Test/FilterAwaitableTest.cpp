#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/FilterAwaitable.h"
#include "PaperUnreal/WeakCoroutine/ValueStream.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFilterAwaitableTest, "PaperUnreal.PaperUnreal.Test.FilterAwaitableTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFilterAwaitableTest::RunTest(const FString& Parameters)
{
	{
		TValueStream<int32> Stream;
		auto Receiver = Stream.GetReceiver();

		int32 Received = 0;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			while (true)
			{
				auto Result = co_await (Stream | Awaitables::FilterIfNotError([](int32 Value) { return Value > 3; }));
				Received = Result;
			}
		});

		for (int32 i = 0; i < 10; i++)
		{
			Receiver.Pin()->ReceiveValue(i);
			TestEqual(TEXT("Filter 함수 테스트: 계속해서 새로 Filter를 만드는 경우"), Received, i > 3 ? i : 0);
		}
	}

	{
		TValueStream<int32> Stream;
		auto Receiver = Stream.GetReceiver();

		int32 Received = 0;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			auto FilteredStream = Stream | Awaitables::FilterIfNotError([](int32 Value) { return Value > 3; });

			while (true)
			{
				auto Result = co_await FilteredStream;
				Received = Result;
			}
		});
		
		for (int32 i = 0; i < 10; i++)
		{
			Receiver.Pin()->ReceiveValue(i);
			TestEqual(TEXT("Filter 함수 테스트: Filter를 한 번만 만들고 계속 쓰는 경우"), Received, i > 3 ? i : 0);
		}
	}

	return true;
}
