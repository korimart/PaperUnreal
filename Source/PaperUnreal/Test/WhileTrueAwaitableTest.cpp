#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PaperUnreal/WeakCoroutine/WhileTrueAwaitable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhileTrueAwaitableTest, "PaperUnreal.PaperUnreal.Test.WhileTrueAwaitableTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWhileTrueAwaitableTest::RunTest(const FString& Parameters)
{
	{
		TLiveData<bool> LiveData;
		bool bStarted = false;

		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			co_await (LiveData.MakeStream() | Awaitables::WhileTrue([&]()
			{
				return [](bool& bStarted) -> FMinimalAbortableCoroutine
				{
					bStarted = true;
					auto F = Finally([&]() { bStarted = false; });
					co_await Awaitables::Forever();
				}(bStarted);
			}));
		});

		for (int32 i = 0; i < 10; i++)
		{
			TestFalse(TEXT(""), bStarted);
			LiveData = true;
			TestTrue(TEXT(""), bStarted);
			LiveData = false;
		}
	}
	
	return true;
}