#include "Misc/AutomationTest.h"
#include "PaperUnreal/SegmentArray.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(SegmentArrayTest, "PaperUnreal.PaperUnreal.Test.SegmentArrayTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool SegmentArrayTest::RunTest(const FString& Parameters)
{
	const auto TestPointsEqual = [&](const auto& Points0, const auto& Points1)
	{
		if (!TestEqual(TEXT(""), Points0.Num(), Points1.Num()))
		{
			return;
		}
		
		for (int32 i = 0; i < Points0.Num(); i++)
		{
			TestEqual(TEXT(""), Points0[i], Points1[i]);
		}
	};

	{
		const TArray<FVector2D> VertexPositions
		{
			{-1.f, 1.f},
			{-1.f, -1.f},
			{1.f, -1.f},
			{1.f, 1.f},
		};

		const TArray<FVector2D> Path
		{
			{-1.f, 0.f},
			{-2.f, 0.f},
			{-2.f, -2.f},
			{2.f, -2.f},
			{2.f, 0.f},
			{1.f, 0.f},
		};

		const TArray<FVector2D> Joined
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

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.ReplacePoints(1, 2, Path);
			TestPointsEqual(SegmentArray.GetPoints(), Joined);
		}

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(Path);
			TestPointsEqual(SegmentArray.GetPoints(), Joined);
		}
	}

	{
		const TArray<FVector2D> VertexPositions
		{
			{-1.f, 1.f},
			{-1.f, -1.f},
			{1.f, -1.f},
			{1.f, 1.f},
		};

		const TArray<FVector2D> Path
		{
			{1.f, 0.f},
			{2.f, 0.f},
			{2.f, 2.f},
			{-2.f, 2.f},
			{-2.f, 0.f},
			{-1.f, 0.f},
		};

		const TArray<FVector2D> Joined
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

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.ReplacePoints(3, 0, Path);
			TestPointsEqual(SegmentArray.GetPoints(), Joined);
		}

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(Path);
			TestPointsEqual(SegmentArray.GetPoints(), Joined);
		}
	}

	{
		const TArray<FVector2D> Square
		{
			{-1.f, 1.f},
			{-1.f, 0.f},
			{-1.f, -1.f},
			{1.f, -1.f},
			{1.f, 1.f},
		};

		FLoopedSegmentArray2D SegmentArray{Square};
		TestTrue(TEXT(""), SegmentArray.IsInside({0.f, 0.f}));
	}

	return true;
}
