#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveDataTest, "PaperUnreal.PaperUnreal.Test.LiveDataTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FLiveDataTest::RunTest(const FString& Parameters)
{
	return true;
}
