#include "Misc/AutomationTest.h"
#include "PaperUnreal/Core/SegmentArray.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(SegmentArrayTest, "PaperUnreal.PaperUnreal.Test.SegmentArrayTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool SegmentArrayTest::RunTest(const FString& Parameters)
{
	const auto TestPointsEqual = [&](auto Text, const auto& Points0, const auto& Points1)
	{
		if (!TestEqual(Text, Points0.Num(), Points1.Num()))
		{
			return;
		}

		for (int32 i = 0; i < Points0.Num(); i++)
		{
			TestEqual(Text, Points0[i], Points1[i]);
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

		// 사각형 영역 + 왼쪽 면에서 시작해서 오른쪽 면에서 끝나는 경로
		// 경로가 커버하는 버텍스 인덱스가 wrap 하지 않는 경우 테스트

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.ReplacePoints(1, 2, Path);
			TestPointsEqual(TEXT("TestCase 0: ReplacePoints"), SegmentArray.GetPoints(), Joined);
		}

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(Path);
			TestPointsEqual(TEXT("TestCase 0: Union"), SegmentArray.GetPoints(), Joined);
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(ReversedPath);
			TestPointsEqual(TEXT("TestCase 0: Reverse Union"), SegmentArray.GetPoints(), Joined);
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

		// 사각형 영역 + 오른쪽 면에서 시작해서 왼쪽 면에서 끝나는 경로
		// 경로가 커버하는 버텍스 인덱스가 wrap 하는 경우 테스트

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.ReplacePoints(3, 0, Path);
			TestPointsEqual(TEXT("TestCase 1: ReplacePoints"), SegmentArray.GetPoints(), Joined);
		}

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(Path);
			TestPointsEqual(TEXT("TestCase 1: Union"), SegmentArray.GetPoints(), Joined);
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(ReversedPath);
			TestPointsEqual(TEXT("TestCase 1: Reverse Union"), SegmentArray.GetPoints(), Joined);
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

		const FLoopedSegmentArray2D SegmentArray{Square};
		TestTrue(TEXT("TestCase 2"), SegmentArray.IsInside({0.f, 0.f}));
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
			{-1.f, 0.5f},
			{-2.f, 0.5f},
			{-2.f, -0.5f},
			{-1.f, -0.5f},
		};

		const TArray<FVector2D> Joined
		{
			{-1.f, 1.f},
			{-1.f, 0.5f},
			{-2.f, 0.5f},
			{-2.f, -0.5f},
			{-1.f, -0.5f},
			{-1.f, -1.f},
			{1.f, -1.f},
			{1.f, 1.f},
		};

		// 왼쪽 면에서 시작해서 왼쪽 면에서 끝나는 경우
		// 같은 면에서 출발해서 도착하는 경우 테스트

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(Path);
			TestPointsEqual(TEXT("TestCase 3: Union"), SegmentArray.GetPoints(), Joined);
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Union(ReversedPath);
			TestPointsEqual(TEXT("TestCase 3: Reverse Union"), SegmentArray.GetPoints(), Joined);
		}
	}

	return true;
}
