// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>
#include "CoreMinimal.h"


struct FMinimalCoroutine
{
	struct promise_type
	{
		FMinimalCoroutine get_return_object()
		{
			return {};
		}

		std::suspend_never initial_suspend() const
		{
			return {};
		}

		std::suspend_never final_suspend() const noexcept
		{
			return {};
		}

		void return_void() const
		{
		}

		void unhandled_exception() const
		{
		}
	};
};
