#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PaperUnreal/WeakCoroutine/WhileTrueAwaitable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhileTrueAwaitableTest, "PaperUnreal.PaperUnreal.Test.WhileTrueAwaitableTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

struct FForever
{
	constexpr bool await_ready() const noexcept
	{
		return false;
	}

	constexpr void await_suspend(const auto&) const noexcept
	{
	}

	constexpr std::monostate await_resume() const noexcept
	{
		return {};
	}

	constexpr void await_abort() const noexcept
	{
	}
};

bool FWhileTrueAwaitableTest::RunTest(const FString& Parameters)
{
	{
		TLiveData<bool> LiveData;
		bool bStarted = false;

		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			co_await (LiveData.CreateStream() | Awaitables::WhileTrue([&]()
			{
				return [](bool& bStarted) -> FMinimalAbortableCoroutine
				{
					bStarted = true;
					auto F = Finally([&]() { bStarted = false; });
					co_await FForever{};
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