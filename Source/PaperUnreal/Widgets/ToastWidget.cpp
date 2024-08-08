// Fill out your copyright notice in the Description page of Project Settings.


#include "ToastWidget.h"

FToastHandle::~FToastHandle()
{
	if (ToastWidget.IsValid())
	{
		ToastWidget->RemoveToastById(Id);
	}
}
