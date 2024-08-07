// Fill out your copyright notice in the Description page of Project Settings.


#include "PrivilegeComponent.h"
#include "PaperUnreal/GameFramework2/Utils.h"

void FConditionalComponents::OnComponentsMaybeChanged()
{
	if (!ComponentOwner.IsValid() || !ComponentGroup.IsValid())
	{
		return;
	}

	if (bCondition)
	{
		for (UClass* Each : Classes)
		{
			if (!HasComponentOfClass(ComponentOwner.Get(), Each))
			{
				ComponentGroup
					->NewChildComponent<UActorComponent2>(ComponentOwner.Get(), Each)
					->RegisterComponent();
			}
		}
	}
	else
	{
		for (UActorComponent* Each : ComponentOwner->GetComponents())
		{
			if (IsOfClass(Each, Classes))
			{
				Each->DestroyComponent();
			}
		}
	}
}
