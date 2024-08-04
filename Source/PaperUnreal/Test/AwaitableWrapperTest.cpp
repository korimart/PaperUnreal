#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwaitableWrapperTest, "PaperUnreal.PaperUnreal.Test.AwaitableWrapperTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwaitableWrapperTest::RunTest(const FString& Parameters)
{
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
