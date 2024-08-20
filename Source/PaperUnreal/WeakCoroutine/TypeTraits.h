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


template <typename WrapperType>
struct TUObjectUnsafeWrapperTypeTraitsImpl
{
};

template <typename ObjectType>
struct TUObjectUnsafeWrapperTypeTraitsImpl<ObjectType*>
{
	using RawPtrType = ObjectType*;
	static ObjectType* GetUObject(ObjectType* Object) { return Object; }
	static ObjectType* GetRaw(ObjectType* Object) { return Object; }
};

template <typename InterfaceType>
struct TUObjectUnsafeWrapperTypeTraitsImpl<TScriptInterface<InterfaceType>>
{
	using RawPtrType = InterfaceType*;
	static UObject* GetUObject(const TScriptInterface<InterfaceType>& Interface) { return Interface.GetObject(); }
	static InterfaceType* GetRaw(const TScriptInterface<InterfaceType>& Interface) { return Interface.GetInterface(); }
};

template <typename InterfaceType>
	requires !std::is_base_of_v<UObject, std::decay_t<InterfaceType>>
	&& requires { typename InterfaceType::UClassType; }
struct TUObjectUnsafeWrapperTypeTraitsImpl<InterfaceType*>
{
	using RawPtrType = InterfaceType*;
	static UObject* GetUObject(InterfaceType* Interface) { return Cast<UObject>(Interface); }
	static InterfaceType* GetRaw(InterfaceType* Interface) { return Interface; }
};


/**
 * UPROPERTY()의 도움 없이는 자신이 dangling pointer인지 스스로 판단할 수 없는 타입들의 Type Traits
 * UObject*, 언리얼 인터페이스 포인터, TScriptInterface 등이 여기에 속할 수 있음
 * TWeakObjectPtr, TSoftObjectPtr는 스스로 IsValid 등을 동해 valid pointer 여부를 체크할 수 있으므로 여기 속하지 않음
 * 
 * @tparam WrapperType UObject를 상속하는 포인터, 언리얼 인터페이스 포인터, TScriptInterface<T> 등등
 */
template <typename WrapperType>
struct TUObjectUnsafeWrapperTypeTraits : TUObjectUnsafeWrapperTypeTraitsImpl<std::decay_t<WrapperType>>
{
};


template <typename TypeList, template <typename> typename TransformTypeFunc>
struct TTransformTypeList;

template <template <typename...> typename TypeList, typename... ElementTypes, template <typename> typename TransformTypeFunc>
struct TTransformTypeList<TypeList<ElementTypes...>, TransformTypeFunc>
{
	using Type = TypeList<typename TransformTypeFunc<ElementTypes>::Type...>;
};


template <typename DelegateType>
struct TDelegateTypeTraits
{
	using ParamTypeTuple = typename TFuncParamTypesToTypeList<TTuple, typename DelegateType::TFuncType>::Type;
	using DecayedParamTypeTuple = typename TTransformTypeList<ParamTypeTuple, TDecay>::Type;
	
	static constexpr int32 ParamCount = TTypeListCount_V<ParamTypeTuple>;
	
	using DecayedParamTypeOrTuple = std::conditional_t<
		ParamCount == 0,
		void,
		typename std::conditional_t<ParamCount == 1, TFirstInTypeList<DecayedParamTypeTuple>, TIdentity<DecayedParamTypeTuple>>::Type>;
};


template <typename T>
concept CUObjectUnsafeWrapper = requires(T Arg) { { TUObjectUnsafeWrapperTypeTraits<T>::GetUObject(Arg) }; };

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


template <typename T>
decltype(auto) AwaitableOrIdentity(T&& MaybeAwaitable)
{
	if constexpr (CAwaitable<T>)
	{
		return Forward<T>(MaybeAwaitable);
	}
	else
	{
		return operator co_await(Forward<T>(MaybeAwaitable));
	}
}


template <typename TupleType>
void ForEachTupleElementIndexed(TupleType&& Tuple, const auto& Func)
{
	Forward<TupleType>(Tuple).ApplyAfter([&]<typename... ElementTypes>(ElementTypes&&... Elements)
	{
		[&]<typename... ElementTypes2, std::size_t... Indices>(std::index_sequence<Indices...>, ElementTypes2&&... Elements2)
		{
			(Func(Indices, Forward<ElementTypes2>(Elements2)), ...);
		}(std::index_sequence_for<ElementTypes...>{}, Forward<ElementTypes>(Elements)...);
	});
}
