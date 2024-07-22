#include "Misc/AutomationTest.h"
#include "PaperUnreal/AreaTracer/SegmentArray.h"

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
			TestTrue(Text, Points0[i].Equals(Points1[i]));
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
			auto Result = SegmentArray.Union(Path);
			TestPointsEqual(TEXT("TestCase 0: Union"), SegmentArray.GetPoints(), Joined);
			TestEqual(TEXT("TestCase 0: Union Result"), Result.Num(), 1);
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			auto Result = SegmentArray.Union(ReversedPath);
			TestPointsEqual(TEXT("TestCase 0: Reverse Union"), SegmentArray.GetPoints(), Joined);
			TestEqual(TEXT("TestCase 0: Union Result"), Result.Num(), 1);
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
			{0.f, 0.f},
			{0.f, -1.f},
		};

		const TArray<FVector2D> Difference
		{
			{-1.f, 1.f},
			{-1.f, 0.f},
			{0.f, 0.f},
			{0.f, -1.f},
			{1.f, -1.f},
			{1.f, 1.f},
		};

		const TArray<FVector2D> ReverseDifference
		{
			{-1.f, -1.f},
			{0.f, -1.f},
			{0.f, 0.f},
			{-1.f, 0.f},
		};

		// 왼쪽 면으로 들어와서 아래 면으로 나가는 경우 테스트

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Difference(Path);
			TestPointsEqual(TEXT("TestCase 4: Difference"), SegmentArray.GetPoints(), Difference);
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Difference(ReversedPath);
			TestPointsEqual(TEXT("TestCase 4: Reverse Differnce"), SegmentArray.GetPoints(), ReverseDifference);
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

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			auto Result = SegmentArray.Clip({{-2.f, 0.f}, {0.f, 0.f}});
			TestEqual(TEXT("TestCase 5: Clip"), FSegment2D{Result}, FSegment2D{{-1.f, 0.f}, {0.f, 0.f}});
		}
	}

	{
		const TArray<FVector2D> VertexPositions
		{
			{-4.f, 2.f},
			{-4.f, 0.f},
			{4.f, 0.f},
			{4.f, 2.f},
			{3.f, 2.f},
			{3.f, 1.f},
			{2.f, 1.f},
			{2.f, 2.f},
			{1.f, 2.f},
			{1.f, 1.f},
			{-1.f, 1.f},
			{-1.f, 2.f},
			{-2.f, 2.f},
			{-2.f, 1.f},
			{-3.f, 1.f},
			{-3.f, 2.f},
		};

		const TArray<FVector2D> Path
		{
			{-5.f, -5.f},
			{5.f, -5.f},
			{5.f, 1.5f},
			{-5.f, 1.5f},
		};

		const TArray<FVector2D> Joined
		{
			{-4.f, 2.f},
			{-4.f, 0.f},
			{4.f, 0.f},
			{4.f, 2.f},
			{3.f, 2.f},
			{3.f, 1.5f},
			{2.f, 1.5f},
			{2.f, 2.f},
			{1.f, 2.f},
			{1.f, 1.5f},
			{-1.f, 1.5f},
			{-1.f, 2.f},
			{-2.f, 2.f},
			{-2.f, 1.5f},
			{-3.f, 1.5f},
			{-3.f, 2.f},
		};

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			auto Result = SegmentArray.Union(Path);
			TestPointsEqual(TEXT("TestCase 6: Union"), SegmentArray.GetPoints(), Joined);
			RETURN_IF_FALSE(TestEqual(TEXT("TestCase 6: Union"), Result.Num(), 3));
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			auto Result = SegmentArray.Union(ReversedPath);
			TestPointsEqual(TEXT("TestCase 6: Reverse Union"), SegmentArray.GetPoints(), Joined);
			RETURN_IF_FALSE(TestEqual(TEXT("TestCase 6: Reverse Union"), Result.Num(), 3));
		}
	}

	{
		const TArray<FVector2D> VertexPositions
		{
			{-4.f, 2.f},
			{-4.f, 0.f},
			{4.f, 0.f},
			{4.f, 2.f},
			{3.f, 2.f},
			{3.f, 1.f},
			{2.f, 1.f},
			{2.f, 2.f},
			{1.f, 2.f},
			{1.f, 1.f},
			{-1.f, 1.f},
			{-1.f, 2.f},
			{-2.f, 2.f},
			{-2.f, 1.f},
			{-3.f, 1.f},
			{-3.f, 2.f},
		};

		const TArray<FVector2D> Path
		{
			{3.f - UE_KINDA_SMALL_NUMBER, 1.5f},
			{-3.f + UE_KINDA_SMALL_NUMBER, 1.5f},
		};

		const TArray<FVector2D> Joined
		{
			{-4.f, 2.f},
			{-4.f, 0.f},
			{4.f, 0.f},
			{4.f, 2.f},
			{3.f, 2.f},
			{3.f, 1.5f},
			{2.f, 1.5f},
			{2.f, 2.f},
			{1.f, 2.f},
			{1.f, 1.5f},
			{-1.f, 1.5f},
			{-1.f, 2.f},
			{-2.f, 2.f},
			{-2.f, 1.5f},
			{-3.f, 1.5f},
			{-3.f, 2.f},
		};

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			auto Result = SegmentArray.Union(Path);
			TestPointsEqual(TEXT("TestCase 7: Union"), SegmentArray.GetPoints(), Joined);
			RETURN_IF_FALSE(TestEqual(TEXT("TestCase 7: Union"), Result.Num(), 3));
		}

		{
			auto ReversedPath = Path;
			std::reverse(ReversedPath.begin(), ReversedPath.end());
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			auto Result = SegmentArray.Union(ReversedPath);
			TestPointsEqual(TEXT("TestCase 7: Reverse Union"), SegmentArray.GetPoints(), Joined);
			RETURN_IF_FALSE(TestEqual(TEXT("TestCase 7: Reverse Union"), Result.Num(), 3));
		}
	}

	{
		const TArray<FVector2D> VertexPositions
		{
			{-10.f, 10.f},
			{-10.f, 0.f},
			{10.f, 0.f},
			{10.f, 10.f},
		};

		const TArray<FVector2D> Path
		{
			{-5.f, -1.f},
			{-5.f, 1.f},
			{-3.f, 1.f},
			{-3.f, -1.f},
			{3.f, -1.f},
			{3.f, 1.f},
			{5.f, 1.f},
			{5.f, -1.f},
		};

		const TArray<FVector2D> Difference
		{
			{-10.f, 10.f},
			{-10.f, 0.f},
			{-5.f, 0.f},
			{-5.f, 1.f},
			{-3.f, 1.f},
			{-3.f, 0.f},
			{3.f, 0.f},
			{3.f, 1.f},
			{5.f, 1.f},
			{5.f, 0.f},
			{10.f, 0.f},
			{10.f, 10.f},
		};

		{
			FLoopedSegmentArray2D SegmentArray{VertexPositions};
			SegmentArray.Difference(Path);
			TestPointsEqual(TEXT("TestCase 8: Difference"), SegmentArray.GetPoints(), Difference);
		}
	}
	return true;
}
