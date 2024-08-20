// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "PaperUnreal/GameFramework2/Utils.h"


class FCoroutineMutex
{
public:
	TCancellableFuture<void> Lock()
	{
		if (!bLocked)
		{
			bLocked = true;
			return {};
		}
		
		Awaiters.Add({});
		return Awaiters.Last().GetFuture();
	}

	void LockChecked()
	{
		check(!bLocked);
		bLocked = true;
	}

	void Unlock()
	{
		if (Awaiters.Num() > 0)
		{
			PopFront(Awaiters).SetValue();
		}
		else
		{
			bLocked = false;
		}
	}

private:
	bool bLocked = false;
	TArray<TCancellablePromise<void>> Awaiters;
};


class FCoroutineScopedLock
{
public:
	FCoroutineScopedLock() = default;
	FCoroutineScopedLock(const FCoroutineScopedLock&) = delete;
	FCoroutineScopedLock& operator=(const FCoroutineScopedLock&) = delete;

	~FCoroutineScopedLock()
	{
		if (Mutex)
		{
			Mutex->Unlock();
		}
	}

	TCancellableFuture<void> Lock(FCoroutineMutex& InMutex)
	{
		Mutex = &InMutex;
		return Mutex->Lock();
	}

	void LockChecked(FCoroutineMutex& InMutex)
	{
		Mutex = &InMutex;
		Mutex->LockChecked();
	}

private:
	FCoroutineMutex* Mutex;
};
