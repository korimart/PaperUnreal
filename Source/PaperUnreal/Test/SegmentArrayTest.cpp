#include "Misc/AutomationTest.h"
#include "PaperUnreal/SegmentArray.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(SegmentArrayTest, "PaperUnreal.PaperUnreal.Test.SegmentArrayTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool SegmentArrayTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> VertexPositions
	{
		{-1.f, 1.f},
		{-1.f, -1.f},
		{1.f, -1.f},
		{1.f, 1.f},
	};

	const TArray<FVector2D> Path0
	{
		{-1.f, 0.f},
		{-2.f, 0.f},
		{-2.f, -2.f},
		{2.f, -2.f},
		{2.f, 0.f},
		{1.f, 0.f},
	};

	const TArray<FVector2D> Path1
	{
		{1.f, 0.f},
		{2.f, 0.f},
		{2.f, 2.f},
		{-2.f, 2.f},
		{-2.f, 0.f},
		{-1.f, 0.f},
	};

	const TArray<FVector2D> Joined0
	{
		{-1.f, 1.f},
		{-1.f, 0.f},
		{-2.f, 0.f},
		{-2.f, -2.f},
		{2.f, -2.f},
		{2.f, -0.f},
		{1.f, -0.f},
		{1.f, 1.f},
	};

	const TArray<FVector2D> Joined1
	{
		{-1.f, -1.f},
		{1.f, -1.f},
		{1.f, 0.f},
		{2.f, 0.f},
		{2.f, 2.f},
		{-2.f, 2.f},
		{-2.f, 0.f},
		{-1.f, 0.f},
	};

	const auto TestPointsEqual = [&](const auto& Points0, const auto& Points1)
	{
		TestEqual(TEXT(""), Points0.Num(), Points1.Num());
		for (int32 i = 0; i < Points0.Num(); i++)
		{
			TestEqual(TEXT(""), Points0[i], Points1[i]);
		}
	};

	{
		FLoopedSegmentArray2D SegmentArray{VertexPositions};
		SegmentArray.ReplacePoints(1, 2, Path0);
		TestPointsEqual(SegmentArray.GetPoints(), Joined0);
	}
	
	{
		FLoopedSegmentArray2D SegmentArray{VertexPositions};
		SegmentArray.ReplacePoints(3, 0, Path1);
		TestPointsEqual(SegmentArray.GetPoints(), Joined1);
	}

	return true;
}
