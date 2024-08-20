// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "PaperUnreal/GameFramework2/Utils.h"


/**
 * 코루틴에서 사용할 수 있는 Mutex로 std::mutex와 달리 싱글 스레드에서만 사용할 수 있습니다.
 * (이 클래스의 목적은 멀티 스레드에 의한 data race를 방지하는 것이 아님)
 * 
 * 싱글 스레드에서 클래스의 데이터가 변경되고 있는 동안에 콜백의 연속 호출에 의해 다시 자기 자신으로 돌아와
 * 마치 data race처럼 변경되고 있는 데이터에 대한 처리가 완료되기 전에 다른 함수가 재차 변경을 시도할 수 있습니다.
 * 
 * 예를 들어 TArray를 ranged for loop로 순회하는 도중 loop 내 델리게이트 호출이 TArray를 Empty하면
 * ranged for loop은 정상적으로 순회를 마칠 수 없고 이것은 에러입니다. 이러한 경우 이 클래스를 사용할 수 있습니다.
 */
class FCoroutineMutex
{
public:
	/**
	 * Lock이 걸렸을 때 완료되는 TCancellableFuture를 반환합니다.
	 */
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

	/**
	 * Lock이 걸려있지 않다고 가정하고 즉시 Lock을 겁니다.
	 */
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


/**
 * @see FCoroutineMutex
 *
 * std::scoped_lock과 비슷하지만 Lock을 명시적으로 호출해야 합니다.
 */
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
