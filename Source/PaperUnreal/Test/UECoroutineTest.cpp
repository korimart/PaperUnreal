#include "UECoroutineTest.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(UECoroutineTest, "PaperUnreal.PaperUnreal.Test.UECoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

#define RETURN_IF_FALSE(boolean) if (boolean) return true;

bool UECoroutineTest::RunTest(const FString& Parameters)
{
	UUECoroutineTestValueProvider* Provider = NewObject<UUECoroutineTestValueProvider>();

	{
		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<bool> Reached;

		RunWeakCoroutine(Temp, [Provider, &Reached, Lifetime = MoveTemp(Lifetime)]() -> FWeakCoroutine
		{
			co_await Provider->FetchValue();
			Reached.Add(true);

			co_await Provider->FetchValue();
			Reached.Add(true);

			co_await Provider->FetchValue();
			Reached.Add(true);

			co_await Provider->FetchValue();
			Reached.Add(true);

			// Should not compile
			// TODO support conversion
			// co_await FUECoroutineTestIncompatibleAwaitable{};
		});

		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 0));
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));
		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 1));
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));
		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));

		Temp->MarkAsGarbage();
		RETURN_IF_FALSE(TestFalse(TEXT(""), *bFreed));

		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
		Provider->IssueValue(0);
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
	}

	return true;
}

#undef RETURN_IF_FALSE
