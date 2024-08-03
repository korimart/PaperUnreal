// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"


template <typename DerivedType>
struct TAwaitableAdaptorBase
{
	template <typename NotAwaitableType> requires !CAwaitable<NotAwaitableType>
	friend decltype(auto) operator|(NotAwaitableType&& NotAwaitable, const DerivedType& Derived)
	{
		return operator co_await(Forward<NotAwaitableType>(NotAwaitable)) | Derived;
	}

	template <typename NotAwaitableType> requires !CAwaitable<NotAwaitableType>
	friend decltype(auto) operator|(NotAwaitableType&& NotAwaitable, DerivedType&& Derived)
	{
		return operator co_await(Forward<NotAwaitableType>(NotAwaitable)) | MoveTemp(Derived);
	}
};
