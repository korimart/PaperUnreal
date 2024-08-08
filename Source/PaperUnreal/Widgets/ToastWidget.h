// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "ToastWidget.generated.h"


class UE_NODISCARD FToastHandle
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


/**
 * 
 */
UCLASS()
class UToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FToastHandle Toast(const FText& Text)
	{
		const uint32 Id = NextId++;
		auto Row = CreateWidget<UToastRowWidget>(this, RowClass);
		Row->SetText(Text);
		IdToWidgetMap.FindOrAdd(Id) = Row;
		ToastPanel->AddChild(Row);
		return {this, Id};
	}

private:
	UPROPERTY(EditAnywhere)
	TSubclassOf<UToastRowWidget> RowClass;

	UPROPERTY(meta=(BindWidget))
	UPanelWidget* ToastPanel;

	UPROPERTY()
	TMap<uint32, UWidget*> IdToWidgetMap;

	uint32 NextId = 0;

	friend class FToastHandle;

	virtual void NativeOnInitialized() override
	{
		Super::NativeOnInitialized();
		ToastPanel->ClearChildren();
	}
	
	void RemoveToastById(uint32 Id)
	{
		if (UWidget* Found = IdToWidgetMap.FindRef(Id))
		{
			Found->RemoveFromParent();
		}
	}
};
