// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
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
	void FindOrSetTeamScore(int32 Index, FLinearColor Color, float Score)
	{
		UTeamScoreWidget* Widget = IndexToWidgetMap.FindRef(Index);
		if (!Widget)
		{
			Widget = CreateWidget<UTeamScoreWidget>(this, ScoreWidgetClass);
			ScorePanel->AddChild(Widget);
			IndexToWidgetMap.Add(Index, Widget);
		}
		
		Widget->SetColorAndScore(Color, Score);
	}

private:
	virtual void NativeOnInitialized() override
	{
		Super::NativeOnInitialized();
		ScorePanel->ClearChildren();
	}
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<UTeamScoreWidget> ScoreWidgetClass;
	
	UPROPERTY(meta=(BindWidget))
	UPanelWidget* ScorePanel;

	UPROPERTY()
	TMap<int32, UTeamScoreWidget*> IndexToWidgetMap;
};
