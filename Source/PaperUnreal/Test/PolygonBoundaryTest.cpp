#include "Misc/AutomationTest.h"
#include "PaperUnreal/AreaMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(PolygonBoundaryTest, "PaperUnreal.PaperUnreal.Test.PolygonBoundaryTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool PolygonBoundaryTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> VertexPositions
	{
		{-1.f, 1.f},
		{-1.f, 0.f},
		{-1.f, -1.f},
		{1.f, -1.f},
		{1.f, 1.f},
	};

	FPolygonBoundary2D PolygonBoundary;
	PolygonBoundary.ReplacePoints(0, 0, VertexPositions);
	TestTrue(TEXT(""), PolygonBoundary.IsInside({0.f, 0.f}));
	
	return true;
}
