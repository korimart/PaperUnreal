// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableAdaptor.h"
#include "TypeTraits.h"


template <typename InnerAwaitableType, typename TransformFuncType>
class TTransformAwaitable
{
public:
	template <typename AwaitableType, typename Trans>
	TTransformAwaitable(AwaitableType&& Awaitable, Trans&& InTransformFunc)
		: InnerAwaitable(MoveTemp(Awaitable)), TransformFunc(Forward<Trans>(InTransformFunc))
	{
	}

	bool await_ready() const
	{
		return InnerAwaitable.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		return InnerAwaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume()
	{
		if constexpr (std::is_void_v<decltype(InnerAwaitable.await_resume())>)
		{
			InnerAwaitable.await_resume();
			return TransformFunc();
		}
		else if constexpr (TIsInstantiationOf_V<decltype(InnerAwaitable.await_resume()), TTuple>)
		{
			return InnerAwaitable.await_resume().ApplyAfter([&]<typename... ArgTypes>(ArgTypes&&... Args)
			{
				return TransformFunc(Forward<ArgTypes>(Args)...);
			});
		}
		else
		{
			return TransformFunc(InnerAwaitable.await_resume());
		}
	}

	void await_abort()
	{
		InnerAwaitable.await_abort();
	}

private:
	InnerAwaitableType InnerAwaitable;
	TransformFuncType TransformFunc;
};

template <typename AwaitableType, typename Trans>
TTransformAwaitable(AwaitableType&& Awaitable, Trans&& TransformFunc) -> TTransformAwaitable<AwaitableType, std::decay_t<Trans>>;


template <typename TransformFuncType>
struct TTransformAdaptor : TAwaitableAdaptorBase<TTransformAdaptor<TransformFuncType>>
{
	std::decay_t<TransformFuncType> Predicate;

	TTransformAdaptor(const std::decay_t<TransformFuncType>& Pred)
		: Predicate(Pred)
	{
	}

	TTransformAdaptor(std::decay_t<TransformFuncType>&& Pred)
		: Predicate(MoveTemp(Pred))
	{
	}

	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TTransformAdaptor Adaptor)
	{
		return TTransformAwaitable{Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.Predicate)};
	}
};


namespace Awaitables_Private
{
	auto TransformIfNotErrorImplVoidCase(const auto& FailableResult, const auto& TransformFunc)
	{
		using TransformedType = decltype(TransformFunc());
		using ReturnType = TFailableResult<TransformedType>;
		return FailableResult ? ReturnType{TransformFunc()} : ReturnType{FailableResult.GetErrors()};
	}

	template <typename TransformFuncType>
	auto TransformIfNotErrorImpl(TransformFuncType&& TransformFunc)
	{
		auto Relay = [TransformFunc = Forward<TransformFuncType>(TransformFunc)]
			<typename FailableResultType>(const FailableResultType& FailableResult)
		{
			// TransformWithError을 사용할 자리에 Transform을 사용하면 여기 걸림
			static_assert(TIsInstantiationOf_V<FailableResultType, TFailableResult>);

			using ResultType = typename FailableResultType::ResultType;

			if constexpr (std::is_void_v<ResultType>)
			{
				// RESEARCH: 이거 인라인으로 쓰면 컴파일이 안 되는데 이유를 잘 모르겠다
				return TransformIfNotErrorImplVoidCase(FailableResult, TransformFunc);
			}
			else if constexpr (TIsInstantiationOf_V<ResultType, TTuple>)
			{
				return [&]<typename... TupleElementTypes>(TTypeList<TupleElementTypes...>)
				{
					using TransformedType = decltype(TransformFunc(std::declval<TupleElementTypes>()...));
					using ReturnType = TFailableResult<TransformedType>;

					if (!FailableResult)
					{
						return ReturnType{FailableResult.GetErrors()};
					}

					return FailableResult.GetResult().ApplyAfter([&]<typename... ArgTypes>(ArgTypes&&... Args)
					{
						return ReturnType{TransformFunc(Forward<ArgTypes>(Args)...)};
					});
				}(typename TSwapTypeList<ResultType, TTypeList>::Type{});
			}
			else
			{
				using TransformedType = std::decay_t<decltype(std::invoke(TransformFunc, FailableResult.GetResult()))>;
				using ReturnType = TFailableResult<TransformedType>;
				return FailableResult ? ReturnType{std::invoke(TransformFunc, FailableResult.GetResult())} : ReturnType{FailableResult.GetErrors()};
			}
		};

		return TTransformAdaptor<decltype(Relay)>{MoveTemp(Relay)};
	}
}


namespace Awaitables
{
	template <typename TransformFuncType>
	auto TransformWithError(TransformFuncType&& TransformFunc)
	{
		return TTransformAdaptor<TransformFuncType>{Forward<TransformFuncType>(TransformFunc)};
	}

	template <typename TransformFuncType>
	auto Transform(TransformFuncType&& TransformFunc)
	{
		return Awaitables_Private::TransformIfNotErrorImpl(Forward<TransformFuncType>(TransformFunc));
	}

	template <typename To>
	auto Cast()
	{
		return Transform([](auto Object) { return ::Cast<To>(Object); });
	}
	
	template <typename To>
	auto CastChecked()
	{
		return Transform([](auto Object) { return ::CastChecked<To>(Object); });
	}

	inline auto IsValid()
	{
		return Transform([](auto Object) { return ::IsValid(Object); });
	}

	template <typename T>
	auto Equals(T&& This)
	{
		return Transform([This = Forward<T>(This)](const auto& Value) { return Value == This; });
	}
	
	inline auto Negate()
	{
		return Transform([](bool bValue) { return !bValue; });
	}

	template <typename ComponentType>
	auto FindComponentByClass()
	{
		return Transform([](auto Actor){ return Actor->template FindComponentByClass<ComponentType>(); });
	}
}
