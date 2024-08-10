// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "TeamScoresWidget.generated.h"


UCLASS()
class UTeamScoreWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetColorAndScore(FLinearColor Color, float Score)
	{
		Image->SetColorAndOpacity(Color);
		Text->SetText(FText::AsNumber(static_cast<int32>(Score / 1000)));
	}

private:
	UPROPERTY(meta=(BindWidget))
	UImage* Image;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* Text;
};


/**
 * 
 */
UCLASS()
class UTeamScoresWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void FindOrSetTeamScore(int32 TeamIndex, FLinearColor Color, float Score)
	{
		FTeamScoreListItem& ListItem = FindOrAddBy(ListItems, TeamIndex, &FTeamScoreListItem::TeamIndex);
		ListItem.TeamIndex = TeamIndex;
		ListItem.Color = Color;
		ListItem.Score = Score;

		Algo::SortBy(ListItems, &FTeamScoreListItem::Score, TGreater<>{});

		RebuildScorePanel();
	}

private:
	struct FTeamScoreListItem
	{
		int32 TeamIndex;
		FLinearColor Color;
		float Score;
	};

	virtual void NativeOnInitialized() override
	{
		Super::NativeOnInitialized();
		ScorePanel->ClearChildren();
	}

	UPROPERTY(EditAnywhere)
	TSubclassOf<UTeamScoreWidget> ScoreWidgetClass;

	UPROPERTY(meta=(BindWidget))
	UPanelWidget* ScorePanel;

	TArray<FTeamScoreListItem> ListItems;

	void RebuildScorePanel()
	{
		while (ScorePanel->GetChildrenCount() < ListItems.Num())
		{
			ScorePanel->AddChild(CreateWidget<UTeamScoreWidget>(this, ScoreWidgetClass));
		}

		while (ScorePanel->GetChildrenCount() > ListItems.Num())
		{
			ScorePanel->RemoveChildAt(0);
		}

		for (int32 i = 0; i < ListItems.Num(); i++)
		{
			Cast<UTeamScoreWidget>(ScorePanel->GetChildAt(i))
				->SetColorAndScore(ListItems[i].Color, ListItems[i].Score);
		}
	}
};
