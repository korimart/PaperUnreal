#include "Misc/AutomationTest.h"
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

		TArray<int32> Received;
		RunWeakCoroutine2([&](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
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
		Array[0].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 1);
		Array[1].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 2);
		Array[2].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 3);
		Array[3].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 4);
		Array[4].Get<0>().SetValue();
		TestEqual(TEXT("void Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 5);
	}
	
	{
		TArray<TTuple<TCancellablePromise<int32>, TCancellableFuture<int32>>> Array;
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());
		Array.Add(MakePromise<int32>());

		TArray<int32> Received;
		RunWeakCoroutine2([&](FWeakCoroutineContext2& Context) -> FWeakCoroutine2
		{
			Received.Add(co_await MoveTemp(Array[0].Get<1>()));
			Received.Add(co_await MoveTemp(Array[1].Get<1>()));
			Received.Add(co_await MoveTemp(Array[2].Get<1>()));
			Received.Add(co_await MoveTemp(Array[3].Get<1>()));
			Received.Add(co_await MoveTemp(Array[4].Get<1>()));
		});

		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received.Num(), 0);
		Array[0].Get<0>().SetValue(0);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[0], 0);
		Array[1].Get<0>().SetValue(1);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[1], 1);
		Array[2].Get<0>().SetValue(2);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[2], 2);
		Array[3].Get<0>().SetValue(3);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[3], 3);
		Array[4].Get<0>().SetValue(4);
		TestEqual(TEXT("int32 Future를 잘 기다릴 수 있는지 테스트"), Received[4], 4);
	}
	
	return true;
}
