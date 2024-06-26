#include "UECoroutineTest.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(UECoroutineTest, "PaperUnreal.PaperUnreal.Test.UECoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

#define RETURN_IF_FALSE(boolean) if (!boolean) return true;

bool UECoroutineTest::RunTest(const FString& Parameters)
{
	UUECoroutineTestValueProvider* Provider = NewObject<UUECoroutineTestValueProvider>();

	{
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<int32> Reached;

		RunWeakCoroutine(Temp, [&]() -> FWeakCoroutine
		{
			Reached.Add(co_await CreateReadyWeakAwaitable(0));
			Reached.Add(co_await CreateReadyWeakAwaitable(1));
			Reached.Add(co_await CreateReadyWeakAwaitable(2));
			Reached.Add(co_await CreateReadyWeakAwaitable(3));
			Reached.Add(co_await CreateReadyWeakAwaitable(4));
		});
		
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 5));
	}

	{
		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<int32> Reached;

		RunWeakCoroutine(Temp, [Provider, &Reached, Lifetime = MoveTemp(Lifetime)]() -> FWeakCoroutine
		{
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
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

	{
		auto Lifetime = MakeUnique<FUECoroutineTestLifetime>();
		TSharedPtr<bool> bFreed = Lifetime->bDestroyed;
		UUECoroutineTestValueProvider* Temp = NewObject<UUECoroutineTestValueProvider>();
		TArray<int32> Reached;

		RunWeakCoroutine(Temp, [Provider, &Reached, Lifetime = MoveTemp(Lifetime)]() -> FWeakCoroutine
		{
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
			Reached.Add(co_await Provider->FetchInt());
		});

		Provider->IssueValue(0);
		Provider->IssueValue(0);
		Provider->ClearRequests();
		RETURN_IF_FALSE(TestEqual(TEXT(""), Reached.Num(), 2));
		RETURN_IF_FALSE(TestTrue(TEXT(""), *bFreed));
	}

	return true;
}

#undef RETURN_IF_FALSE
