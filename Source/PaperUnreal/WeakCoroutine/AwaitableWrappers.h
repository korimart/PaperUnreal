// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalCoroutine.h"
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

private:
	InnerAwaitableType InnerAwaitable;
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


template <CErrorReportingAwaitable InnerAwaitableType>
class TAbortIfErrorAwaitable
	: public TConditionalResumeAwaitable<TAbortIfErrorAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TAbortIfErrorAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	template <typename AwaitableType>
	TAbortIfErrorAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CNonErrorReportingAwaitable<TAbortIfErrorAwaitable>);
	}

	bool ShouldResume(const auto& Handle, const TFailableResult<ResultType>& Result) const
	{
		if (Result.Failed())
		{
			Handle.promise().SetErrors(Result.GetErrors());
			return false;
		}

		return true;
	}

	ResultType await_resume()
	{
		return Super::await_resume().GetResult();
	}
};

template <typename AwaitableType>
TAbortIfErrorAwaitable(AwaitableType&&) -> TAbortIfErrorAwaitable<AwaitableType>;


struct FAbortIfErrorAdaptor
{
	template <typename AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, FAbortIfErrorAdaptor)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TAbortIfErrorAwaitable{Forward<AwaitableType>(Awaitable)};
		}
	}
};


template <CErrorReportingAwaitable InnerAwaitableType, typename AllowedErrorTypeList>
class TAbortIfErrorNotInAwaitable
	: public TConditionalResumeAwaitable<TAbortIfErrorNotInAwaitable<InnerAwaitableType, AllowedErrorTypeList>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TAbortIfErrorNotInAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	static constexpr bool bAllowAll = std::is_same_v<void, AllowedErrorTypeList>;

	template <typename AwaitableType>
	TAbortIfErrorNotInAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CErrorReportingAwaitable<TAbortIfErrorNotInAwaitable>);
	}

	bool ShouldResume(const auto& Handle, const TFailableResult<ResultType>& Result) const
	{
		if (Result.Succeeded())
		{
			return true;
		}

		if constexpr (!bAllowAll)
		{
			if ([&]<typename... AllowedErrorTypes>(TTypeList<AllowedErrorTypes...>)
			{
				return !Result.template OnlyContains<AllowedErrorTypes...>();
			}(AllowedErrorTypeList{}))
			{
				Handle.promise().SetErrors(Result.GetErrors());
				return false;
			}
		}

		return true;
	}
};


template <typename AllowedErrorTypeList>
struct TAbortIfErrorNotInAdaptor
{
	template <typename AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, TAbortIfErrorNotInAdaptor)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TAbortIfErrorNotInAwaitable<AwaitableType, AllowedErrorTypeList>{Forward<AwaitableType>(Awaitable)};
		}
	}
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


// TODO 이거 여러번 호출 가능한 stream으로 변경
// TODO TAllOfAwaitable 구현
// TODO 위 둘 구현을 위해서는 외부에서의 코루틴 종료를 위해 inner가 핸들에 destroy 호출 시 코루틴이 살아있을 때만
// destroy를 하도록 하는 wrapper awaitable을 작성해야 함 -> 이러면 promise_type도 이 기능을 지원해야 함으로 다중 상속 이용
// -> awaitable에서 promise_type에 직접 액세스 하는 경우는 전부 다중 상속으로 만들 수 있을 것 같다
// Future 반환하는 코루틴도 다중 상속으로 해결 가능할 듯;;
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

	// TODO stream일 필요가 없음 여러번 await 가능한 awaitable이면 됨
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
// TODO 단순화 가능
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


namespace Awaitables
{
	inline FAbortIfErrorAdaptor AbortIfError()
	{
		return {};
	}
	
	template <typename AllowedErrorTypeList>
	TAbortIfErrorNotInAdaptor<AllowedErrorTypeList> AbortIfErrorNotIn()
	{
		return {};
	}
}


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
