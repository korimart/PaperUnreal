#include "UECoroutineTest.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(UECoroutineTest, "PaperUnreal.PaperUnreal.Test.UECoroutineTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool UECoroutineTest::RunTest(const FString& Parameters)
{
	UUECoroutineTestObject* TestObject = NewObject<UUECoroutineTestObject>();
	TestObject->DoSomeCoroutine();
	TestObject->Proxy->SetValue(42);
	
	return true;
}
