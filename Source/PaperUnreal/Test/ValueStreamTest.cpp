#include "Misc/AutomationTest.h"
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
					Stream::MakeFromMulticastDelegate<int32>(Delegate0).Get<0>(),
					Stream::MakeFromMulticastDelegate<int32>(Delegate1).Get<0>(),
					Stream::MakeFromMulticastDelegate<int32>(Delegate2).Get<0>())
				| Awaitables::AbortIfError()
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

	return true;
}
