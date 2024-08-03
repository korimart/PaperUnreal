// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"


template <typename Derived, CAwaitable InnerAwaitableType>
class TConditionalResumeAwaitable
{
public:
	using ReturnType = decltype(std::declval<InnerAwaitableType>().await_resume());

	template <typename AwaitableType>
	TConditionalResumeAwaitable(AwaitableType&& Awaitable)
		: InnerAwaitable(Forward<AwaitableType>(Awaitable))
	{
	}

	TConditionalResumeAwaitable(TConditionalResumeAwaitable&&) = default;
	
	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	bool await_suspend(const HandleType& Handle)
	{
		if (InnerAwaitable.await_ready())
		{
			if (!ShouldResume(Handle))
			{
				Handle.destroy();
				return true;
			}

			return false;
		}

		struct FConditionalResumeHandle
		{
			TConditionalResumeAwaitable* This;
			HandleType Handle;

			void resume() const { This->ShouldResume(Handle) ? Handle.resume() : Handle.destroy(); }
			void destroy() const { Handle.destroy(); }
			auto& promise() const { return Handle.promise(); }
		};

		using SuspendType = decltype(std::declval<InnerAwaitableType>()
			.await_suspend(std::declval<FConditionalResumeHandle>()));

		if constexpr (std::is_void_v<SuspendType>)
		{
			InnerAwaitable.await_suspend(FConditionalResumeHandle{this, Handle});
			return true;
		}
		else if (InnerAwaitable.await_suspend(FConditionalResumeHandle{this, Handle}))
		{
			return true;
		}

		if (!ShouldResume(Handle))
		{
			Handle.destroy();
			return true;
		}

		return false;
	}

	ReturnType await_resume()
	{
		return MoveTemp(*Return);
	}

protected:
	InnerAwaitableType InnerAwaitable;

private:
	TOptional<ReturnType> Return;

	template <typename HandleType>
	bool ShouldResume(const HandleType& Handle)
	{
		struct FReadOnlyHandle
		{
			FReadOnlyHandle(HandleType InHandle)
				: Handle(InHandle)
			{
			}

			auto& promise() const
			{
				return Handle.promise();
			}

		private:
			HandleType Handle;
		};

		Return = InnerAwaitable.await_resume();

		return static_cast<Derived*>(this)->ShouldResume(
			FReadOnlyHandle{Handle}, static_cast<const ReturnType&>(*Return));
	}
};