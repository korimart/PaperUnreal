// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SolidColorMaterial.generated.h"


USTRUCT()
struct FSolidColorMaterial
{
	GENERATED_BODY()

	FSolidColorMaterial() = default;

	void SetBaseColorMaterial(UMaterial* BaseColorMaterial)
	{
		Material = UMaterialInstanceDynamic::Create(BaseColorMaterial, GetTransientPackage());
		UpdateMaterialParameterValues();
	}

	void SetColor(const FLinearColor& InColor)
	{
		Color = InColor;
		UpdateMaterialParameterValues();
	}

	void SetOpacity(float InOpacity)
	{
		Opacity = InOpacity;
		UpdateMaterialParameterValues();
	}

	UMaterialInstanceDynamic* Get() const { return Material; }

private:
	UPROPERTY()
	UMaterialInstanceDynamic* Material = nullptr;
	
	FLinearColor Color = FLinearColor::Black;
	float Opacity = 1.f;

	void UpdateMaterialParameterValues()
	{
		if (IsValid(Material))
		{
			static FName ColorParamName{TEXT("Base Color")};
			Material->SetVectorParameterValue(ColorParamName, Color);

			static FName OpacityParamName{TEXT("Opacity")};
			Material->SetScalarParameterValue(OpacityParamName, Opacity);
		}
	}
};
