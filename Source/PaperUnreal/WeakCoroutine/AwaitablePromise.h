// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"


template <typename ReturnType>
class TAwaitablePromise
{
public:
	template <typename U>
	void return_value(U&& Value)
	{
		this->Promise.SetValue(Forward<U>(Value));
	}

protected:
	TCancellableFuture<ReturnType> GetFuture()
	{
		return Promise.GetFuture();
	}
	
private:
	TCancellablePromise<ReturnType> Promise;
};


template <>
class TAwaitablePromise<void>
{
public:
	void return_void()
	{
		this->Promise.SetValue();
	}
	
protected:
	TCancellableFuture<void> GetFuture()
	{
		return Promise.GetFuture();
	}
	
private:
	TCancellablePromise<void> Promise;
};