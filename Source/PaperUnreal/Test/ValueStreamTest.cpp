#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/ValueStream.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FValueStreamTest, "PaperUnreal.PaperUnreal.Test.ValueStreamTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FValueStreamTest::RunTest(const FString& Parameters)
{
	{
		DECLARE_MULTICAST_DELEGATE_OneParam(FSomeDelegate, int32);
		FSomeDelegate Delegate0;
		FSomeDelegate Delegate1;
		FSomeDelegate Delegate2;

		TArray<int32> Received;
		FWeakCoroutine Coroutine = RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			auto Added = Stream::Combine(
					MakeStreamFromDelegate(Delegate0),
					MakeStreamFromDelegate(Delegate1),
					MakeStreamFromDelegate(Delegate2))
				| Awaitables::Transform([](int32 A, int32 B, int32 C) { return A + B + C; });

			while (true)
			{
				Received.Add(co_await Added);
			}
		});

		TestEqual(TEXT("Stream::Combine 테스트"), Received.Num(), 0);
		Delegate0.Broadcast(1);
		TestEqual(TEXT("Stream::Combine 테스트"), Received.Num(), 0);
		Delegate1.Broadcast(2);
		TestEqual(TEXT("Stream::Combine 테스트"), Received.Num(), 0);
		Delegate2.Broadcast(3);
		TestEqual(TEXT("Stream::Combine 테스트"), Received[0], 6);
		Delegate2.Broadcast(3);
		TestEqual(TEXT("Stream::Combine 테스트"), Received[1], 6);
		Delegate1.Broadcast(3);
		TestEqual(TEXT("Stream::Combine 테스트"), Received[2], 7);
		Delegate0.Broadcast(3);
		TestEqual(TEXT("Stream::Combine 테스트"), Received[3], 9);
	}

	{
		TLiveData<int32> LiveData0;
		TLiveData<int32> LiveData1;
		TLiveData<int32> LiveData2;

		TArray<bool> Received;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			auto AllEven = Stream::AllOf(
				LiveData0.MakeStream() | Awaitables::Transform([](int32 Value) { return Value % 2 == 0; }),
				LiveData1.MakeStream() | Awaitables::Transform([](int32 Value) { return Value % 2 == 0; }),
				LiveData2.MakeStream() | Awaitables::Transform([](int32 Value) { return Value % 2 == 0; }));

			while (true)
			{
				Received.Add(co_await AllEven);
			}
		});

		TestEqual(TEXT("Stream::AllOf 테스트"), Received[0], true);

		LiveData0 = 1;
		TestEqual(TEXT("Stream::AllOf 테스트"), Received[1], false);
		LiveData1 = 1;
		LiveData2 = 1;

		LiveData0 = 2;
		LiveData1 = 1;
		LiveData2 = 1;
		TestEqual(TEXT("Stream::AllOf 테스트"), Received.Num(), 2);

		LiveData0 = 2;
		LiveData1 = 1;
		LiveData2 = 2;
		TestEqual(TEXT("Stream::AllOf 테스트"), Received.Num(), 2);

		LiveData0 = 2;
		LiveData1 = 2;
		LiveData2 = 2;
		TestEqual(TEXT("Stream::AllOf 테스트"), Received[2], true);
		TestEqual(TEXT("Stream::AllOf 테스트"), Received.Num(), 3);
	}

	return true;
}
