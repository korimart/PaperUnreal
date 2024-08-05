#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/AllOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAllOfAwaitableTest, "PaperUnreal.PaperUnreal.Test.AllOfAwaitableTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAllOfAwaitableTest::RunTest(const FString& Parameters)
{
	{
		TLiveData<bool> LiveData0;
		TLiveData<bool> LiveData1;
		TLiveData<bool> LiveData2;

		TArray<int32> Received;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			auto AllTrue = Awaitables::AllOf(LiveData0.CreateStream(), LiveData1.CreateStream(), LiveData2.CreateStream());
			
			while (true)
			{
				co_await AllTrue;
				Received.Add(42);
			}
		});

		TestEqual(TEXT(""), Received.Num(), 0);
		LiveData0 = true;
		TestEqual(TEXT(""), Received.Num(), 0);
		LiveData2 = true;
		TestEqual(TEXT(""), Received.Num(), 0);
		LiveData1 = true;
		TestEqual(TEXT(""), Received.Num(), 1);
	}

	return true;
}
