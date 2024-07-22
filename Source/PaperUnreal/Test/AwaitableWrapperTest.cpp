#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
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

	return true;
}
