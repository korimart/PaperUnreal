// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "ToastWidget.generated.h"


class FToastHandle
{
public:
	FToastHandle(class UToastWidget* Widget, uint32 InId)
		: ToastWidget(Widget), Id(InId)
	{
	}

	FToastHandle(FToastHandle&& Other) noexcept
		: ToastWidget(Other.ToastWidget), Id(Other.Id)
	{
		Other.ToastWidget = nullptr;
	}

	FToastHandle& operator=(FToastHandle&& Other) = delete;

	~FToastHandle();

private:
	TWeakObjectPtr<UToastWidget> ToastWidget;
	uint32 Id;
};


/**
 * 
 */
UCLASS()
class UToastRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void SetText(const FText& Text);
};


USTRUCT()
struct FToastRowWithIdAndPriority
{
	GENERATED_BODY()
	
	UPROPERTY()
	UToastRowWidget* RowWidget;

	uint32 Id;
	int32 Priority;
};


/**
 * 
 */
UCLASS()
class UToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FToastHandle Toast(const FText& Text, int32 InPriority = 0)
	{
		const uint32 Id = NextId++;
		auto Row = CreateWidget<UToastRowWidget>(this, RowClass);
		Row->SetText(Text);

		const int32 Index = Algo::LowerBoundBy(ToastRows, InPriority, &FToastRowWithIdAndPriority::Priority, TGreater<>{});
		ToastRows.Insert({Row, Id, InPriority}, Index);
		RebuildToastPanel();
		
		return {this, Id};
	}

private:
	UPROPERTY(EditAnywhere)
	TSubclassOf<UToastRowWidget> RowClass;

	UPROPERTY(meta=(BindWidget))
	UPanelWidget* ToastPanel;

	UPROPERTY()
	TArray<FToastRowWithIdAndPriority> ToastRows;

	uint32 NextId = 0;

	friend class FToastHandle;

	virtual void NativeOnInitialized() override
	{
		Super::NativeOnInitialized();
		RebuildToastPanel();
	}

	void RebuildToastPanel()
	{
		ToastPanel->ClearChildren();
		for (const FToastRowWithIdAndPriority& Each : ToastRows)
		{
			ToastPanel->AddChild(Each.RowWidget);
		}
	}
	
	void RemoveToastById(uint32 Id)
	{
		if (FToastRowWithIdAndPriority* Found = Algo::FindBy(ToastRows, Id, &FToastRowWithIdAndPriority::Id))
		{
			Found->RowWidget->RemoveFromParent();
			ToastRows.RemoveAll([Id = Found->Id](const auto& Each){ return Each.Id == Id; });
		}
	}
};
