// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


template <typename...>
struct TFalse
{
	static constexpr bool Value = false;
};


template <typename...>
struct TTypeList
{
};


template <typename, template <typename...> typename>
struct TIsInstantiationOf : std::false_type
{
};

template <typename... Args, template <typename...> typename T>
struct TIsInstantiationOf<T<Args...>, T> : std::true_type
{
};

template <typename T, template <typename...> typename Template>
inline constexpr bool TIsInstantiationOf_V = TIsInstantiationOf<std::decay_t<T>, Template>::value;


template <typename T, template <typename...> typename Template>
concept CInstantiationOf = TIsInstantiationOf_V<T, Template>;

template <typename T, template <typename...> typename Template>
concept CNotInstantiationOf = !TIsInstantiationOf_V<T, Template>;


template <typename FromType, typename... ToTypes>
constexpr bool TIsConvertible_V = (std::is_convertible_v<FromType, ToTypes> || ...);


template <template <typename...> typename TypeList, typename SignatureType>
struct TFuncParamTypesToTypeList;

template <template <typename...> typename TypeList, typename RetType, typename... ParamTypes>
struct TFuncParamTypesToTypeList<TypeList, RetType(ParamTypes...)>
{
	using Type = TypeList<ParamTypes...>;
};


template <typename TypeList>
struct TFirstInTypeList;

template <template <typename...> typename TypeList, typename First, typename... Types>
struct TFirstInTypeList<TypeList<First, Types...>>
{
	using Type = First;
};


template <typename SrcTypeList, template <typename...> typename DestTypeList>
struct TSwapTypeList;

template <template <typename...> typename SrcTypeList, template <typename...> typename DestTypeList, typename... Types>
struct TSwapTypeList<SrcTypeList<Types...>, DestTypeList>
{
	using Type = DestTypeList<Types...>;
};


template <typename TypeList>
constexpr int32 TTypeListCount_V = 0;

template <template <typename...> typename TypeList, typename... Types>
constexpr int32 TTypeListCount_V<TypeList<Types...>> = sizeof...(Types);


template <typename FromType, typename... ToTypes>
struct TFirstConvertibleType;

template <typename FromType, typename ToType, typename... ToTypes>
struct TFirstConvertibleType<FromType, ToType, ToTypes...>
{
	using Type = typename std::conditional_t<
		std::is_convertible_v<FromType, ToType>,
		TIdentity<ToType>,
		TFirstConvertibleType<FromType, ToTypes...>>::Type;
};


template <typename FunctorType>
struct TGetReturnType
{
private:
	using FuncType = typename TDecay<FunctorType>::Type;

	template <typename F, typename Ret, typename... ArgTypes>
	static Ret Helper(Ret (F::*)(ArgTypes...));

	template <typename F, typename Ret, typename... ArgTypes>
	static Ret Helper(Ret (F::*)(ArgTypes...) const);

public:
	using Type = decltype(Helper(&FuncType::operator()));
};


template <typename T>
concept CDelegate = TIsInstantiationOf_V<T, TDelegate>;

template <typename T>
concept CMulticastDelegate = TIsInstantiationOf_V<T, TMulticastDelegate>;

template <typename AwaitableType>
concept CAwaitable = requires(AwaitableType Awaitable) { Awaitable.await_resume(); };

template <typename T>
concept CAwaitableConvertible = requires(T Arg) { operator co_await(Arg); };

template <typename T, typename U>
concept CEqualityComparable = requires(T Arg0, U Arg1) { Arg0 == Arg1; };

template <typename T, typename U>
concept CInequalityComparable = requires(T Arg0, U Arg1) { Arg0 != Arg1; };

template <typename PredicateType, typename ValueType>
concept CPredicate = requires(PredicateType Predicate, ValueType Value) { { Predicate(Value) } -> std::same_as<bool>; };
