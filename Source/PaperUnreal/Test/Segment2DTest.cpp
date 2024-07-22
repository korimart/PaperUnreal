#include "Misc/AutomationTest.h"
#include "PaperUnreal/AreaTracer/SegmentArray.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(Segment2DTest, "PaperUnreal.PaperUnreal.Test.Segment2DTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool Segment2DTest::RunTest(const FString& Parameters)
{
	const FSegment2D Segment{{1.f, 1.f}, {2.f, 1.f}};
	const FSegment2D Perp = Segment.Perp({1.5f, 2.f});
	TestEqual(TEXT(""), Perp, {{1.5f, 1.f}, {1.5f, 2.f}});
	return true;
}
