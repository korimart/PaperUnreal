// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaActor.h"
#include "Core/ActorComponent2.h"
#include "AreaSpawnerComponent.generated.h"


class FAreaSpawnLocationCalculator
{
public:
	void Configure(const FBox2D& World, float InCellSideLength)
	{
		CellSideLength = InCellSideLength;

		const FVector2D WidthHeight = World.GetSize();
		CellXCount = WidthHeight.X / CellSideLength;
		CellYCount = WidthHeight.Y / CellSideLength;

		const float XMargin = (WidthHeight.X - CellXCount * CellSideLength) / 2.f;
		const float YMargin = (WidthHeight.Y - CellYCount * CellSideLength) / 2.f;
		CellStartX = World.Min.X + XMargin;
		CellStartY = World.Min.Y + YMargin;

		ClearGrid();
	}

	void ClearGrid()
	{
		EmptyCells.Empty();
		for (int32 i = 0; i <= CellIndex1D(CellXCount, CellYCount); i++)
		{
			EmptyCells.Add(i);
		}
	}

	void OccupyGrid(const FLoopedSegmentArray2D& Area)
	{
		TSet<int32> OccupiedCells;
		const FBox2D AreaBoundingBox = Area.CalculateBoundingBox();
		ForEachCellInBox(AreaBoundingBox, [&](int32 LinearIndex, const FBox2D& Cell)
		{
			const FVector2D LeftTop{Cell.Min.X, Cell.Max.Y};
			const FVector2D LeftBottom{Cell.Min.X, Cell.Min.Y};
			const FVector2D RightBottom{Cell.Max.X, Cell.Min.Y};
			const FVector2D RightTop{Cell.Max.X, Cell.Max.Y};
			const FSegment2D Edge0{LeftTop, LeftBottom};
			const FSegment2D Edge1{LeftBottom, RightBottom};
			const FSegment2D Edge2{RightBottom, RightTop};
			const FSegment2D Edge3{RightTop, LeftTop};

			if (Cell.IsInside(AreaBoundingBox)
				|| Area.IsInside(LeftTop)
				|| Area.IsInside(LeftBottom)
				|| Area.IsInside(RightBottom)
				|| Area.IsInside(RightTop)
				|| Area.FindIntersection(Edge0)
				|| Area.FindIntersection(Edge1)
				|| Area.FindIntersection(Edge2)
				|| Area.FindIntersection(Edge3))
			{
				OccupiedCells.Add(LinearIndex);
			}
		});

		EmptyCells = EmptyCells.Difference(OccupiedCells);
	}

	TOptional<FBox2D> GetRandomEmptyCell() const
	{
		if (EmptyCells.Num() == 0)
		{
			return {};
		}

		return GetCell(EmptyCells.Array()[FMath::RandRange(0, EmptyCells.Num() - 1)]);
	}

private:
	float CellStartX = 0.f;
	int32 CellXCount = 0;
	float CellStartY = 0.f;
	int32 CellYCount = 0;
	float CellSideLength = 0.f;
	TSet<int32> EmptyCells;

	int32 CellIndex1D(int32 X, int32 Y) const
	{
		return FMath::Min(Y, CellYCount - 1) * CellXCount + FMath::Min(X, CellXCount - 1);
	}

	TTuple<int32, int32> CellIndex2D(int32 Index) const
	{
		const int32 Y = Index / CellYCount;
		const int32 X = Index % CellYCount;
		return MakeTuple(X, Y);
	}

	FBox2D GetCell(int32 Index) const
	{
		const auto [X, Y] = CellIndex2D(Index);
		return GetCell(X, Y);
	}

	FBox2D GetCell(int32 X, int32 Y) const
	{
		const FVector2D Min{CellStartX + X * CellSideLength, CellStartY + Y * CellSideLength};
		const FVector2D Max{Min.X + CellSideLength, Min.Y + CellSideLength};
		return {Min, Max};
	}

	template <typename FuncType>
	void ForEachCellInBox(const FBox2D& Box, const FuncType& Func) const
	{
		const int32 FirstXIndex = (Box.Min.X - CellStartX) / CellSideLength;
		const int32 FirstYIndex = (Box.Min.Y - CellStartY) / CellSideLength;
		for (int32 Y = FirstYIndex; Y < CellYCount; Y++)
		{
			for (int32 X = FirstXIndex; X < CellXCount; X++)
			{
				Func(CellIndex1D(X, Y), GetCell(X, Y));
			}
		}
	}
};


UCLASS()
class UAreaSpawnerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	const TValueStreamer<AAreaActor*>& GetSpawnedAreaStreamer() const
	{
		return SpawnedAreaStreamer;
	}

	AAreaActor* SpawnAreaAtRandomEmptyLocation()
	{
		check(GetNetMode() != NM_Client);

		SpawnLocationCalculator.ClearGrid();
		for (AAreaActor* Each : RepSpawnedAreas)
		{
			const auto AreaBoundary = Each->FindComponentByClass<UAreaBoundaryComponent>();
			SpawnLocationCalculator.OccupyGrid(*AreaBoundary->GetBoundaryStreamer().GetValue());
		}

		if (TOptional<FBox2D> CellToSpawnAreaIn = SpawnLocationCalculator.GetRandomEmptyCell())
		{
			const FVector SpawnLocation{CellToSpawnAreaIn->GetCenter(), 50.f};

			AAreaActor* Ret = GetWorld()->SpawnActor<AAreaActor>(SpawnLocation, {});
			RepSpawnedAreas.Add(Ret);
			return Ret;
		}

		return nullptr;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedAreas)
	TArray<AAreaActor*> RepSpawnedAreas;

	TValueStreamer<AAreaActor*> SpawnedAreaStreamer;
	FAreaSpawnLocationCalculator SpawnLocationCalculator;

	UAreaSpawnerComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		// TODO get this from an actor in the world
		SpawnLocationCalculator.Configure(
			{FVector2D::Zero(), {3000.f, 3000.f}}, 300.f);
	}

	UFUNCTION()
	void OnRep_SpawnedAreas() { SpawnedAreaStreamer.ReceiveValuesIfNotInHistory(RepSpawnedAreas); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepSpawnedAreas);
	}
};
