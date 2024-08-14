// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/MinimalCoroutine.h"


class FSolidColorMaterial
{
public:
	static TCancellableFuture<FSolidColorMaterial> Create(const FLinearColor& Color)
	{
		auto [Promise, Future] = MakePromise<FSolidColorMaterial>();

		[](TCancellablePromise<FSolidColorMaterial> Promise, FLinearColor Color) -> FMinimalCoroutine
		{
			static const TSoftObjectPtr<UMaterial> BaseColorMaterial
			{
				FSoftObjectPath{TEXT("/Script/Engine.Material'/Game/LevelPrototyping/Materials/M_Solid.M_Solid'")}
			};

			if (auto Material = co_await RequestAsyncLoad(BaseColorMaterial))
			{
				FSolidColorMaterial Ret{UMaterialInstanceDynamic::Create(Material.GetResult(), GetTransientPackage())};
				Ret.SetColor(Color);
				Promise.SetValue(Ret);
			}
		}(MoveTemp(Promise), Color);

		return MoveTemp(Future);
	}

	void SetColor(const FLinearColor& Color)
	{
		static FName ColorParamName{TEXT("Base Color")};
		Material->SetVectorParameterValue(ColorParamName, Color);
	}

	UMaterialInstanceDynamic* Get() const { return Material.Get(); }

private:
	TStrongObjectPtr<UMaterialInstanceDynamic> Material;

	FSolidColorMaterial(UMaterialInstanceDynamic* Instance)
		: Material(Instance)
	{
	}
};
