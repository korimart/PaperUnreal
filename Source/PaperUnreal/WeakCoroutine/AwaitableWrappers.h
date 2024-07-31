// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <source_location>

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalCoroutine.h"
#include "TypeTraits.h"


template <typename AwaitableType>
concept CErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	requires CAwaitable<AwaitableType>;
	{ Awaitable.await_resume() } -> CInstantiationOf<TFailableResult>;
};


template <typename AwaitableType>
concept CNonErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	requires CAwaitable<AwaitableType>;
	requires !CErrorReportingAwaitable<AwaitableType>;
};


template <CErrorReportingAwaitable AwaitableType>
struct TErrorReportResultType
{
	using Type = typename decltype(std::declval<AwaitableType>().await_resume())::ResultType;
};


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

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	bool await_suspend(const HandleType& Handle)
	{
		if (InnerAwaitable.await_ready())
		{
			const bool bResume = ShouldResume(Handle);
			if (!bResume)
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

			void resume() const
			{
				const bool bResume = This->ShouldResume(Handle);
				bResume ? Handle.resume() : Handle.destroy();
			}

			void destroy() const { Handle.destroy(); }
		};

		using SuspendType = decltype(std::declval<InnerAwaitableType>().await_suspend(std::declval<FConditionalResumeHandle>()));

		if constexpr (std::is_void_v<SuspendType>)
		{
			InnerAwaitable.await_suspend(FConditionalResumeHandle{this, Handle});
			return true;
		}
		else if (InnerAwaitable.await_suspend(FConditionalResumeHandle{this, Handle}))
		{
			return true;
		}

		const bool bResume = ShouldResume(Handle);
		if (!bResume)
		{
			Handle.destroy();
			return true;
		}
		return false;
	}

private:
	InnerAwaitableType InnerAwaitable;

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

		Derived* AsDerived = static_cast<Derived*>(this);
		return AsDerived->ShouldResume(FReadOnlyHandle{Handle}, InnerAwaitable.await_resume());
	}
};


template <CErrorReportingAwaitable InnerAwaitableType>
class TErrorRemovingAwaitable : public TConditionalResumeAwaitable<TErrorRemovingAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TErrorRemovingAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	template <typename AwaitableType>
	TErrorRemovingAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CNonErrorReportingAwaitable<TErrorRemovingAwaitable>);
	}

	bool ShouldResume(const auto& Handle, TFailableResult<ResultType>&& InnerResult)
	{
		Result = MoveTemp(InnerResult);

		if (Result->Failed())
		{
			Handle.promise().SetErrors(Result->GetErrors());
			return false;
		}

		return true;
	}

	ResultType await_resume()
	{
		return MoveTempIfPossible(Result->GetResult());
	}

private:
	TOptional<TFailableResult<ResultType>> Result;
};

template <typename AwaitableType>
TErrorRemovingAwaitable(AwaitableType&&) -> TErrorRemovingAwaitable<AwaitableType>;


struct FAllowAll
{
};


template <CErrorReportingAwaitable InnerAwaitableType, typename AllowedErrorTypeList, typename NeverAllowedTypeList>
class TErrorFilteringAwaitable
	: public TConditionalResumeAwaitable
	<
		TErrorFilteringAwaitable<InnerAwaitableType, AllowedErrorTypeList, NeverAllowedTypeList>,
		InnerAwaitableType
	>
{
public:
	using Super = TConditionalResumeAwaitable<TErrorFilteringAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	static constexpr bool bAllowAll = std::is_same_v<FAllowAll, AllowedErrorTypeList>;

	template <typename AwaitableType>
	TErrorFilteringAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CErrorReportingAwaitable<TErrorFilteringAwaitable>);
	}

	bool ShouldResume(const auto& Handle, TFailableResult<ResultType>&& InnerResult)
	{
		Result = MoveTemp(InnerResult);

		if (Result->Succeeded())
		{
			return true;
		}

		if constexpr (!bAllowAll)
		{
			if ([&]<typename... AllowedErrorTypes>(TTypeList<AllowedErrorTypes...>)
			{
				return !Result->template OnlyContains<AllowedErrorTypes...>();
			}(AllowedErrorTypeList{}))
			{
				Handle.promise().SetErrors(Result->GetErrors());
				return false;
			}
		}

		if ([&]<typename... NeverAllowedErrorTypes>(TTypeList<NeverAllowedErrorTypes...>)
		{
			return Result->template ContainsAnyOf<NeverAllowedErrorTypes...>();
		}(NeverAllowedTypeList{}))
		{
			Handle.promise().SetErrors(Result->GetErrors());
			return false;
		}

		return true;
	}

	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(*Result);
	}

private:
	TOptional<TFailableResult<ResultType>> Result;
};


template <CNonErrorReportingAwaitable InnerAwaitableType>
class TAlwaysResumingAwaitable
{
public:
	using ResultType = decltype(std::declval<InnerAwaitableType>().await_resume());

	template <typename AwaitableType>
	TAlwaysResumingAwaitable(AwaitableType&& Awaitable)
		: Inner(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CErrorReportingAwaitable<TAlwaysResumingAwaitable>);
	}

	bool await_ready() const
	{
		return Inner.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		struct FAbortChecker
		{
			TAlwaysResumingAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() const { Handle.resume(); }

			void destroy() const
			{
				This->bAborted = true;
				Handle.resume();
			}
		};

		bAborted = false;
		return Inner.await_suspend(FAbortChecker{this, Forward<HandleType>(Handle)});
	}

	TFailableResult<ResultType> await_resume()
	{
		if (bAborted)
		{
			return UErrorReportingError::InnerAwaitableAborted();
		}

		return Inner.await_resume();
	}

private:
	bool bAborted = false;
	InnerAwaitableType Inner;
};

template <typename AwaitableType>
TAlwaysResumingAwaitable(AwaitableType&&) -> TAlwaysResumingAwaitable<AwaitableType>;


template <typename AwaitableType>
struct TEnsureErrorReporting
{
	using Type = TAlwaysResumingAwaitable<AwaitableType>;
};

template <CErrorReportingAwaitable AwaitableType>
struct TEnsureErrorReporting<AwaitableType>
{
	using Type = AwaitableType;
};


template <typename... InnerAwaitableTypes>
class TAnyOfAwaitable
{
public:
	using InnerAwaitableTuple = TTuple<typename TEnsureErrorReporting<InnerAwaitableTypes>::Type...>;
	using ResultType = int32;

	template <typename... AwaitableTypes>
	TAnyOfAwaitable(AwaitableTypes&&... Awaitables)
		: InnerAwaitables(Forward<AwaitableTypes>(Awaitables)...)
	{
		static_assert(CErrorReportingAwaitable<TAnyOfAwaitable>);
	}

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	void await_suspend(const HandleType& Handle)
	{
		TSharedRef<int32> Counter = MakeShared<int32>(sizeof...(InnerAwaitableTypes));
		ForEachTupleElementIndexed(InnerAwaitables, [&](int32 Index, auto& Awaitable)
		{
			[](auto Awaitable, int32 Index, HandleType Handle,
			   TSharedRef<int32> ResultIndex, TSharedRef<int32> CleanUpCounter) -> FMinimalCoroutine
			{
				auto Result = co_await Awaitable;

				--(*CleanUpCounter);

				if (*ResultIndex < 0 && Result.Succeeded())
				{
					*ResultIndex = Index;
					Handle.resume();
					co_return;
				}

				if (*ResultIndex < 0 && *CleanUpCounter <= 0)
				{
					Handle.resume();
				}
			}(MoveTemp(Awaitable), Index, Handle, ResultIndex, Counter);
		});
	}

	TFailableResult<int32> await_resume()
	{
		if (*ResultIndex >= 0)
		{
			return *ResultIndex;
		}

		return NewError(TEXT("TAnyOfAwaitable 아무도 에러 없이 완료하지 않았음"));
	}

private:
	InnerAwaitableTuple InnerAwaitables;
	TSharedRef<int32> ResultIndex = MakeShared<int32>(-1);
};

template <typename... AwaitableTypes>
TAnyOfAwaitable(AwaitableTypes&&...) -> TAnyOfAwaitable<AwaitableTypes...>;


template <typename ValueStreamType, typename PredicateType>
class TFilteringAwaitable
{
public:
	using ResultType = typename std::decay_t<ValueStreamType>::ResultType;

	template <typename Stream, typename Pred>
	TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
		: ValueStream(Forward<Stream>(ValueStream)), Predicate(Forward<Pred>(Predicate))
	{
		static_assert(CErrorReportingAwaitable<TFilteringAwaitable>);
	}

	bool await_ready() const
	{
		return false;
	}

	void await_suspend(auto Handle)
	{
		[](TFilteringAwaitable& This, auto Handle) -> FMinimalCoroutine
		{
			while (true)
			{
				auto Result = co_await This.ValueStream;
				if (Result.Failed() || This.Predicate(Result.GetResult()))
				{
					This.Ret = MoveTemp(Result);
					break;
				}
			}

			Handle.resume();
		}(*this, Handle);
	}

	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(Ret.GetValue());
	}

private:
	ValueStreamType ValueStream;
	PredicateType Predicate;
	TOptional<TFailableResult<ResultType>> Ret;
};

template <typename Stream, typename Pred>
TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
	-> TFilteringAwaitable<std::conditional_t<std::is_lvalue_reference_v<Stream>, Stream, std::decay_t<Stream>>, std::decay_t<Pred>>;


template <typename AwaitableType, typename TransformFuncType>
class TTransformingAwaitable
{
public:
	template <typename Trans>
	TTransformingAwaitable(AwaitableType&& Inner, Trans&& InTransformFunc)
		: Awaitable(MoveTemp(Inner)), TransformFunc(Forward<Trans>(InTransformFunc))
	{
	}

	bool await_ready() const
	{
		return Awaitable.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		return Awaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume()
	{
		if constexpr (std::is_void_v<decltype(Awaitable.await_resume())>)
		{
			Awaitable.await_resume();
			return TransformFunc();
		}
		else
		{
			return TransformFunc(Awaitable.await_resume());
		}
	}

private:
	AwaitableType Awaitable;
	TransformFuncType TransformFunc;
};

template <typename Await, typename Trans>
TTransformingAwaitable(Await&& Awaitable, Trans&& TransformFunc)
	-> TTransformingAwaitable<std::decay_t<Await>, std::decay_t<Trans>>;


template <typename InnerAwaitableType>
class TSourceLocationAwaitable
{
public:
	template <typename AwaitableType>
	TSourceLocationAwaitable(AwaitableType&& Awaitable)
		: InnerAwaitable(Forward<AwaitableType>(Awaitable))
	{
	}
	
	bool await_ready() const
	{
		return InnerAwaitable.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle, std::source_location SL = std::source_location::current())
	{
		Handle.promise().SetSourceLocation(SL);
		return InnerAwaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume()
	{
		return InnerAwaitable.await_resume();
	}
	
private:
	InnerAwaitableType InnerAwaitable;
};

template <typename AwaitableType>
TSourceLocationAwaitable(AwaitableType&&) -> TSourceLocationAwaitable<AwaitableType>;


template <typename... MaybeAwaitableTypes>
auto AnyOf(MaybeAwaitableTypes&&... MaybeAwaitables)
{
	return TAnyOfAwaitable{AwaitableOrIdentity(Forward<MaybeAwaitableTypes>(MaybeAwaitables))...};
}


template <typename ValueStreamType, typename PredicateType>
auto Filter(ValueStreamType&& Stream, PredicateType&& Predicate)
{
	return TFilteringAwaitable{Forward<ValueStreamType>(Stream), Forward<PredicateType>(Predicate)};
}


template <typename MaybeAwaitableType, typename TransformFuncType>
auto Transform(MaybeAwaitableType&& MaybeAwaitable, TransformFuncType&& TransformFunc)
{
	return TTransformingAwaitable
	{
		AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable)),
		Forward<TransformFuncType>(TransformFunc),
	};
}
