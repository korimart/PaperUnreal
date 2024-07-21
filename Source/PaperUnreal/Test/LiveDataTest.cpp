#include "CancellableFutureTest.h"
#include "Misc/AutomationTest.h"
#include "PaperUnreal/Core/LiveData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveDataTest, "PaperUnreal.PaperUnreal.Test.LiveDataTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FLiveDataTest::RunTest(const FString& Parameters)
{
	// UDummy* Dummy = NewObject<UDummy>();
	// TLiveData<UDummy*> DummyLiveData{Dummy};
	//
	// TestTrue(TEXT(""), DummyLiveData.Get() == Dummy);
	// Dummy = nullptr;
	// TestTrue(TEXT(""), DummyLiveData.Get() == Dummy);
	
	return true;
}
