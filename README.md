# C++20 코루틴을 활용한 언리얼 엔진 이벤트 기반 프로그래밍

## Build

- MSVC v14.38-17.8
- Unreal Engine 5.4

## 영상으로 설명 보기

본 리포지토리에 대한 설명은 아래 링크에서 영상으로도 확인하실 수 있습니다.

https://www.youtube.com/watch?v=QxqItTpRW_c

## 개요

Weak Coroutine은 C++20의 코루틴 기능을 언리얼 엔진에 접목한 코드 플러그인입니다. Weak Coroutine의 목적은 언리얼 엔진의 기존 델리게이트 기반 이벤트 프로그래밍을 코루틴을 통해 발전시켜 콜백 기반의 프로그래밍 방법이 가지는 문제점들을 해결하고 실수하기 어려우면서도 가독성이 뛰어난 코드를 쉽게 작성할 수 있는 토대를 마련하는 것입니다. 이 문서에서는 Weak Coroutine의 사용 방법과 Weak Coroutine이 해결하는 문제에 대해서 설명하고 이후 Weak Coroutine의 구현 방법과 같은 기술적인 상세에 대해서도 설명합니다.

이 리포지토리는 Weak Coroutine의 구현과 Weak Coroutine의 사용을 보여드리기 위한 샘플 게임으로 구성되어 있습니다. 먼저 Weak Coroutine의 설명 이후 Weak Coroutine의 구체적인 활용 모습을 샘플 게임을 통해 시연합니다.

## 목차

- Weak Coroutine 소개
- Weak Coroutine의 활용
- Weak Coroutine의 구현 방법
- 결론

## Weak Coroutine으로 작성한 코드의 모습

다음은 Weak Coroutine을 사용하여 작성할 수 있는 코드 예시입니다.

```C++
FWeakCoroutine InitiateScoreboardSequence()
{
    // 유저가 스코어 표시 버튼을 누를 때까지 기다린다.
    co_await OnScoreButtonPressed;

    // 위젯을 생성해 화면에 표시한다.
    UScoreBoardWidget* ScoreboardWidget = CreateWidget<UScoreBoardWidget>(...);
    ScoreboardWidget->AddToViewport();

    while (true)
    {
        // 아직 표시할 스코어가 없으므로 스로버를 표시한다.
        ScoreboardWidget->ShowThrobber();

        // 서버에서 스코어를 가져온다.
        const FScoreboard Scores = co_await ScoreComponent->FetchScores();

        // 스코어 보드에 표시한다.
        ScoreboardWidget->ShowScores(Scores);

        // 유저의 다음 인풋을 기다린다.
        const int32 Selection = co_await Awaitables::AnyOf(
            ScoreboardWidget->OnRefreshButtonPressed,
            ScoreboardWidget->OnCancelButtonPressed);

        // 새로고침을 눌렀으면 스코어 가져오는 로직을 반복한다.
        // 취소를 눌렀으면 루프를 탈출한다.
        if (Selection == 1)
        {
            break;
        }
    }

    // 유저가 취소를 눌렀으므로 위젯을 닫는다.
    ScoreboardWidget->RemoveFromParent();
}
```

위 코드가 정확히 무슨 일을 어떠한 방식으로 처리하는지에 대해 지금부터 알아보겠습니다.

## Weak Coroutine의 기본적인 사용법

### Weak Coroutine의 실행

Weak Coroutine을 실행하는 방법에는 두 가지가 있습니다.

#### `RunWeakCoroutine` 함수를 사용한 실행

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        // 여기에 코루틴 바디를 작성합니다.
    });
}
```

`RunWeakCoroutine` 함수는 첫 번째 파라미터로 코루틴이 실행되는 동안 살아있어야 하는 `UObject`의 포인터를 받습니다. 델리게이트와 멀티캐스트 델리게이트의 `CreateWeakLambda` 또는 `AddWeakLambda`의 첫 번째 파라미터의 역할과 비슷합니다. 이 포인터가 가리키는 객체가 파괴되면 코루틴도 더 이상 진행되지 않습니다. 이에 대해서는 아래 단락에서 좀 더 자세히 설명합니다. 일반적으로 `BeginPlay`와 같이 `UObject` 클래스의 멤버 함수에서 코루틴을 실행하는 경우 `this`를 첫번째 파라미터로 넘겨 멤버 변수들에 대한 안전한 접근을 보장 받을 수 있습니다.

두 번째 파라미터로는 코루틴 바디에 해당하는 lambda expression을 받습니다. return type은 `FWeakCoroutine`으로 명시되어야 합니다.

#### 멤버 함수를 사용한 실행

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    InitiateCoroutine();
}

FWeakCoroutine InitiateCoroutine()
{
    // 여기에 코루틴 바디를 작성합니다.
}
```

멤버 함수로 실행할 경우 return type만 `FWeaKCoroutine`으로 설정해주면 `RunWeakCoroutine`에 `this`를 넘겨 실행할 때와 동일한 효과를 얻을 수 있습니다. 즉 `this`가 파괴된 이후에는 코루틴이 진행되지 않음이 자동으로 보장됩니다.

### Weak Coroutine의 Execution Flow

Weak Coroutine의 실행을 일시중지하는 가장 간단한 방법은 델리게이트에 `co_await`하는 것입니다.

```C++
FSimpleMulticastDelegate OnSomethingHappened;
```

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("BeginPlay가 호출되었습니다."));

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        UE_LOG(LogTemp, Warning, TEXT("코루틴에 일단 진입합니다."));

        // 멀티캐스트 델리게이트의 브로드캐스트를 기다린다.
        co_await OnSomethingHappened;

        UE_LOG(LogTemp, Warning, TEXT("델리게이트가 브로드캐스트 되었습니다."));
    });

    UE_LOG(LogTemp, Warning, TEXT("코루틴이 중단되었기 때문에 BeginPlay를 마저 실행합니다."));
}
```

Weak Coroutine은 실행이 되면 기본적으로 진행할 수 있는 동안에는 최대한 진행합니다. 그리고 `co_await`를 만나면 더 진행할 수 있는지를 검사합니다. 멀티캐스트 델리게이트의 `operator co_await`은 항상 진행을 불허하는 Awaitable을 반환하므로 위 예시의 코루틴은 `co_await OnSomethingHappened;` 라인에서 중지됩니다. 그리고 실행 흐름을 호출자, 이 경우 `BeginPlay` 함수에게 돌려줍니다. 이후 임의의 시점에 `OnSomethingHappened`가 브로드캐스트 되면 코루틴이 깨어나 `co_await OnSomethingHappened;` 라인의 아래 줄부터 실행을 재개합니다.

`BeginPlay`가 호출된 이후 아웃풋 로그는 다음과 같습니다.

```
BeginPlay가 호출되었습니다.
코루틴에 일단 진입합니다.
코루틴이 중단되었기 때문에 BeginPlay를 마저 실행합니다.
```

그리고 이후 아래 코드가 호출되면

```C++
OnSomethingHappened.Broadcast();
```

아웃풋 로그는 다음과 같아집니다.

```
BeginPlay가 호출되었습니다.
코루틴에 일단 진입합니다.
코루틴이 중단되었기 때문에 BeginPlay를 마저 실행합니다.
델리게이트가 브로드캐스트 되었습니다.
```

### `TCancellableFuture`와 `TCancellablePromise`

Weak Coroutine은 일반적으로 `co_await`를 할 때 `TCancellableFuture`를 대상으로 기다리게 됩니다. 위 예시에서 보여드렸던 델리게이트에 대한 `co_await`도 `CancellableFuture.h` 파일에서 다음과 같이 정의되어 있습니다.

```C++
/**
 * co_await Delegate; 지원용
 */
template <CDelegate DelegateType>
auto operator co_await(DelegateType& Delegate)
{
	return TCancellableFutureAwaitable{MakeFutureFromDelegate(Delegate)};
}

/**
 * co_await MulticastDelegate; 지원용
 */
template <CMulticastDelegate MulticastDelegateType>
auto operator co_await(MulticastDelegateType& MulticastDelegate)
{
	return TCancellableFutureAwaitable{MakeFutureFromDelegate(MulticastDelegate)};
}
```

즉 델리게이트에 대해 `co_await`를 하면 델리게이트가 `MakeFutureFromDelegate`함수에 의해 `TCancellableFuture`로 변환되어 반환되므로 사실은 `TCancellableFuture`에 `co_await`을 한 것과 동일합니다.

`TCancellableFuture`는 `std::future` 내지는 `TFuture`를 Weak Coroutine의 기능에 맞추어 변경한 것입니다. `Promise`를 통해 `Future`를 얻을 수 있고 `Future`에 `Then`을 붙일 수 있는 등 기본적인 인터페이스는 `std::future` 또는 `TFuture`와 동일합니다.

Weak Coroutine에서는 다음과 같이 사용할 수 있습니다.

```C++
/**
 * 서버에서 현재 스코어를 가져옵니다.
 */
TCancellableFuture<FScoreboard> FetchScores();
```

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        // 서버에서 스코어를 가져온다.
        const FScoreboard Scores = co_await ScoreComponent->FetchScores();

        // 스코어로 무언가를 한다.
        // ...
    });
}
```

`FetchScores`함수의 구현에서는 `std::promise` 또는 `TPromise`와 동일하게 `TCancellablePromise<FScoreboard>`를 생성하고 `TCancellableFuture<FScoreboard>`를 `GetFuture()`로 얻어 반환합니다. 그리고 스코어가 준비된 시점에 Promise를 통해 값을 공급해주면 반환한 Future가 완료되면서 코루틴을 깨우고 코루틴의 실행을 재개합니다.

### Weak Coroutine의 에러 핸들링 - `TFailableResult`

Weak Coroutine에는 에러 핸들링 기능이 구현되어 있어 `co_await`에서 에러가 발생했을 때에 대한 처리를 추가하거나 추가하지 않을 수 있습니다.

`ErrorReporting.h` 파일에서 `TFailableResult` 클래스의 정의를 찾아보실 수 있는데 `TFailableResult`는 `TOptional`와 개념적으로 유사하므로 비교해서 설명드리겠습니다.

`TOptional`은 값이 있을 때는 `IsSet()` 상태를 가지며 `Get()` 함수를 사용해 값에 접근하는 것이 가능합니다. 값이 없을 때는 `IsSet()`이 `false`를 반환하므로 값에 접근할 수 없습니다.

`TFailableResult`도 마찬가지로 값이 있음과 값이 없음 두 상태를 가집니다. 차이점은 값이 없음 상태일 때 왜 값이 없는지 그 이유에 접근할 수 있다는 점입니다. 해당 이유는 `UFailableResultError`를 상속하는 오브젝트의 형태로 저장되어 있으므로 언리얼 엔진의 여러 리플렉션 기능을 활용하여 에러의 명세를 확인할 수 있습니다.

Weak Coroutine은 이 `TFailableResult`를 활용해 에러 핸들링 기능을 구현합니다. Weak Coroutine은 먼저 `co_await` expression의 타입이 무엇인지 검사합니다. 그리고 해당 타입이 `TFailableResult`이면 에러가 발생했는지 여부를 검사합니다. 만약 에러가 발생했으면 코루틴을 재개하지 않고 즉시 종료합니다. 에러 없이 성공적으로 값이 반환되었으면 `TFailableResult<T>` 타입에서 `TFailableResult` 부분을 제거하고 `T`를 반환한 후 코루틴을 재개합니다. 예시로 보여드리면 다음과 같습니다.


```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        // 서버에서 스코어를 가져온다.
        const FScoreboard Scores = co_await ScoreComponent->FetchScores();

        // 스코어로 무언가를 한다.
        // ...
    });
}
```

아까와 완벽히 똑같은 예시라는 것을 눈치채셨을 것 같습니다. 같은 예시를 또 가져온 이유는 `TCancellableFuture<T>`의 반환이 `T`가 아니라 사실 `TFailableResult<T>`이기 때문입니다. 여기서 일어나는 일을 순서대로 정리하면 다음과 같습니다.

1. `FetchScores` 함수가 `TCancellableFuture<FScoreboard>`를 반환하고 여기에 `co_await`를 하면서 기다림이 시작됨
1. 값 또는 에러가 클라이언트에 도착하면 Future가 완료되면서 코루틴 재개를 시도함
1. Weak Coroutine은 코루틴 재개를 위해 Future가 반환한 `TFailableResult<FScoreboard>`가 값을 가지는지 에러를 가지는지를 검사함
    1. 에러인 경우 : 코루틴을 재개하지 않음 -> 코루틴 파괴
    1. 값인 경우 : `TFailableResult<FScoreboard>`에서 `FScoreboard`를 빼내서 이것으로 코루틴 재개

만약 에러가 발생될 것이 사전에 기대되고 에러를 우아하게(gracefully) 처리하고자 하는 경우에는 다음과 같이 작성할 수 있습니다.

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        // 서버에서 스코어를 가져온다.
        const TFailableResult<FScoreboard> Scores
            = (co_await ScoreComponent->FetchScores() | Awaitables::Catch<UPageNotFound404>());

        if (Scores)
        {
            ShowScores(Scores.GetResult());
        }
        else
        {
            ShowErrorMessage(TEXT("페이지를 찾을 수 없습니다"));
        }
    });
}
```

위 예시에서는 `FetchScores` 함수가 `UPageNotFound404` 에러를 반환할 수 있다고 기대하고 `FetchScores` 함수 뒤에 `| Awaitables::Catch<UPageNotFound404>()`를 붙여, 만약 발생한 에러가 이 에러라면 코루틴을 종료하지 말 것을 Weak Coroutine에게 알리고 있습니다. 에러를 캐치할 경우에는 값이 아닌 에러로 코루틴이 재개되는 것도 가능하기 때문에 반환도 `FScoreboard`에서 `TFailableResult<FScoreboard>`로 변경되었습니다. 만약 `UPageNotFound404` 이외의 에러가 발생했다면 코루틴은 재개되지 않고 파괴됩니다. `Catch` 함수의 템플릿 파라미터로 더 많은 에러 타입을 넘기거나 `CatchAll()` 함수를 대신 붙여 모든 에러에 대해 항상 코루틴을 재개할 것을 지정할 수도 있습니다.

### Weak Coroutine의 Object Life Time Management

비동기 프로그래밍에서 가장 중요한 고려 사항 중 하나는 비동기로 콜백이 호출되었을 때 오브젝트가 살아있는지 여부입니다.

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        USomeObject* SomeObject1 = NewObject<USomeObject>();
        USomeObject* SomeObject2 = NewObject<USomeObject>();

        co_await OnSomethingHappened;

        DoSomething(SomeObject1, SomeObject2);
    });
}
```

위 코드의 문제점은 코루틴이 정지된 이후 재개되는 시점, 즉 `OnSomethingHappened` 델리게이트가 브로드캐스트 되는 시점이 지금이 아닌 미래라는 점입니다. 해당 브로드캐스트가 발생하는 미래에는 `SomeObject1`과 `SomeObject2`의 생존에 관한 어떠한 보장도 존재하지 않습니다. 코루틴이 `co_await`에서 기다리는 사이에 GC가 발생하면 위 오브젝트 포인터들은 dangling pointer가 되고 이후 이 포인터들에 대한 접근은 안전하지 않습니다.

위 문제는 델리게이트에서도 발생할 수 있는 문제로, 기존의 해결 방법은 아래처럼 `TWeakObjectPtr`를 사용해 접근 전 생사를 확인하는 것입니다.

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        TWeakObjectPtr<USomeObject> SomeObject1 = NewObject<USomeObject>();
        TWeakObjectPtr<USomeObject> SomeObject2 = NewObject<USomeObject>();

        co_await OnSomethingHappened;

        if (!SomeObject1.IsValid() || !SomeObject2.IsValid())
        {
            co_return;
        }

        DoSomething(SomeObject1.Get(), SomeObject2.Get());
    });
}
```

이 방식의 문제점은 계속되는 `IsValid` 체크로 인해 코드 가독성이 떨어진다는 점과 `TWeakObjectPtr`를 선언하는 것을 프로그래머가 매번 까먹지 않아야 하는 것이 강제되는 등 휴먼 에러 가능성을 높인다는 점입니다. 이는 `co_await`가 여러 개 존재하는 상황에서 더욱 두드러집니다.

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        TWeakObjectPtr<USomeObject> SomeObject1 = NewObject<USomeObject>();
        TWeakObjectPtr<USomeObject> SomeObject2 = NewObject<USomeObject>();

        co_await OnSomethingHappened;

        if (!SomeObject1.IsValid() || !SomeObject2.IsValid())
        {
            co_return;
        }

        DoSomething(SomeObject1.Get(), SomeObject2.Get());

        co_await OnSomethingHappened;

        if (!SomeObject1.IsValid() || !SomeObject2.IsValid())
        {
            co_return;
        }

        DoSomething(SomeObject1.Get(), SomeObject2.Get());

        co_await OnSomethingHappened;

        if (!SomeObject1.IsValid() || !SomeObject2.IsValid())
        {
            co_return;
        }

        DoSomething(SomeObject1.Get(), SomeObject2.Get());
    });
}
```

`co_await`를 한 번 지날 때마다 오브젝트의 생존이 보장되지 않으므로 똑같은 체크 로직이 반복적으로 도입되어야 합니다. 코드에서 게임 로직보다도 생존확인이 더 많은 비중을 차지하는 주객전도가 발생할 수 있다는 뜻입니다. 이를 방지하기 위해 Weak Coroutine은 validity 체크를 자동화했습니다.

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        TAbortPtr<USomeObject> SomeObject1 = co_await NewObject<USomeObject>();
        TAbortPtr<USomeObject> SomeObject2 = co_await NewObject<USomeObject>();

        co_await OnSomethingHappened;
        DoSomething(SomeObject1, SomeObject2);

        co_await OnSomethingHappened;
        DoSomething(SomeObject1, SomeObject2);

        co_await OnSomethingHappened;
        DoSomething(SomeObject1, SomeObject2);
    });
}
```

위 코드는 수동으로 Validity를 체크하던 위의 코드와 기능적으로 동일합니다. 차이점은 이 경우 Weak Coroutine이 `co_await`에서 깨어날 때마다 자동으로 validity를 체크해준다는 것입니다.

Weak Coroutine에서는 `UObject`에 대한 포인터에 대해 `co_await`를 호출할 경우 해당 포인터를 Weak List라고 하는 배열에 넣고 `TAbortPtr`를 반환합니다. Weak Coroutine은 코루틴이 깨어나야 할 때마다 먼저 Weak List 안에 들어있는 오브젝트들을 체크합니다. 배열 내부의 오브젝트들은 `TWeakObjectPtr`로 저장되므로 임의의 시점에 생존여부를 검사하는 것이 가능합니다. 그리고 이 검사를 통해 배열의 오브젝트 중 하나라도 파괴된 상태라는 것이 확인되면 코루틴의 정상적인 실행이 불가능하다고 가정하고 코루틴을 재개하는 대신 파괴합니다. 이 Weak List에는 `RunWeakCoroutine`의 첫 번째 파라미터로 넘겨진 `UObject*`와, 멤버 함수로 Weak Coroutine을 실행한 경우에는 `this`도 자동으로 들어가기 때문에 Weak Coroutine 내부에서 `this`에 대한 접근이 안전하게 됩니다.

반환되는 `TAbortPtr`의 역할은 해당 오브젝트가 코루틴 내에서 여전히 접근 가능한 스코프 내에 있는지 판단하기 위함입니다. 만약 if문 내에서만 사용되는 변수가 있다면 if문 바깥에서는 해당 변수에 더 이상 접근할 수 없습니다. 접근할 수 없다면 접근이 안전한지 여부를 검사할 필요도 없습니다. `TAbortPtr`는 자신이 살아있는 동안에만 자신이 감싸는 포인터를 Weak List에 넣습니다. 자신이 파괴되는 시점, 즉 스코프가 종료되는 시점에 자신이 감싸는 포인터를 Weak List에서 제거함으로써 코루틴이 불필요한 validity 체크로 인해 의도치 않게 종료 및 파괴되는 것을 방지합니다.

### `TLiveData`를 통한 비동기 데이터 관리

Weak Coroutine 폴더 내에는 Weak Coroutine의 구현 뿐만 아니라 Weak Coroutine과 더불어 사용할 수 있는 여러 클래스들이 동봉되어 있습니다. 그 중 하나가 `TLiveData`입니다. `TLiveData`는 안드로이드 프로그래밍에서 사용하는 `LiveData` 클래스를 모방하여 C++에서 작성하고 그 위에 Weak Coroutine 내에서 사용하기 위한 여러 기능을 추가한 것입니다.

`TLiveData`는 데이터 그리고 그 데이터가 변경될 때마다 호출되는 델리게이트를 하나의 클래스로 합친 것과 개념적으로 동일합니다. 즉 `TLiveData`의 멤버 함수를 통해서 `TLiveData`가 현재 들고 있는 데이터를 접근하는 것도, 이 데이터가 변경될 때마다 실행 받을 콜백 함수를 등록하는 것도 가능합니다. `int32`를 예로 들면 다음과 같습니다.

```C++
TLiveData<int32> CurrentHealth;

// 체력 조회
const int32 Health = CurrentHealth.Get();

// 체력 변경될 때마다 할 일 등록
CurrentHealth.Observe(this, [this](int32 Health)
{
    DoSomethingWithHealth(Health);
});

// 체력 수정 (값에 변화가 있으면 콜백 호출됨)
CurrentHealth = 100;
```

현재 체력을 직접 멤버 변수로 선언하고 이에 대한 델리게이트를 선언, Getter와 Setter를 작성해 체력이 변경될 때마다 델리게이트를 브로드캐스트하는 로직은 너무나도 많이 등장하는 패턴이기 때문에 `TLiveData`로 캡슐화함으로써 코드 재사용성을 높일 수 있습니다.

`TLiveData`가 안드로이드의 `LiveData`와 차별화되는 가장 큰 특징은 Weak Coroutine 내부에서 사용함으로써 콜백 등록 방식의 단점을 회피하고 코루틴의 장점을 살릴 수 있다는 것입니다.

```C++
// 현재 체력
TLiveData<int32> CurrentHealth;
```

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        // 컴파일 되지 않음
        const int32 Health = co_await CurrentHealth; // ??
    });
}
```

우선 단순히 위와 같이 `co_await`를 호출해보면 컴파일이 되지 않을 것입니다. 왜냐하면 `TLiveData`는 항상 내부에 데이터를 들고 있기 때문에 비동기적으로 값을 기다릴 필요 없이 바로 `Get()`함수를 호출해 즉시 값을 얻을 수 있기 때문입니다.

하지만 위와 같은 사용법이 가능한 경우가 있습니다. 바로 `TLiveData`가 내부에 들고 있는 데이터가 Valid 체크가 가능한 타입의 데이터일 때입니다. 이러한 타입에는 `TOptional`이나 `UObject*` 등이 있습니다. 구체적으로 어떠한 타입이 가능한지에 대해서는 `LiveData.h` 상단의 `TLiveDataValidator` 클래스의 specialization들을 참고 부탁드립니다.

액터 포인터를 예시로 들어보겠습니다.

```C++
TLiveData<ABossMonster*> LastBoss;
```

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        TAbortPtr<ABossMonster> Boss = co_await LastBoss;
        ShowMessage(TEXT("라스트 보스가 출현했습니다"));
        Boss->Roar();
    });
}
```

`ABossMonster*`는 Valid와 Invalid 두 상태를 가질 수 있기 때문에 int32의 경우와 달리 `co_await` 대상이 될 수 있습니다. 라스트 보스는 라스트 보스 방에 들어가기 전까지는 스폰되어 있지 않을 수 있습니다. 그렇기 때문에 `TLiveData<ABossMonster*> LastBoss;`는 처음에는 `nullptr`를 들고 있으므로 이 때 상태는 invalid 입니다. 이러한 타입의 `TLiveData`에 `co_await`을 호출하면 데이터가 valid 상태가 되는 시점에 완료되는 `TCancellableFuture`를 반환합니다. 즉 Weak Coroutine에서 데이터가 valid 해졌을 때 이것으로 무언가를 하는 로직을 작성할 수 있습니다.

위 예시 코드의 흐름은 다음과 같습니다.

1. 이미 라스트 보스 액터가 스폰되어 있는 경우 -> `co_await`이 대기 없이 즉시 라스트 보스 액터 포인터로 코루틴을 재개하므로 출현 메세지가 나타남.
1. 라스트 보스 액터가 현재 존재하지 않는 경우 -> `TLiveData`에 라스트 보스 액터 포인터가 세팅되는 순간에 코루틴이 재개되면서 출현 메세지가 나타남.

이와 같은 기능은 멀티플레이 Replication 환경에서 특히 유용하게 사용할 수 있습니다. 멀티플레이 환경에서는 서버에서 액터를 생성하면 클라이언트에서 이에 대한 메세지를 받아 액터를 생성하는데, 액터가 다르다면 메세지의 도착 순서에 대한 어떠한 보장도 기대할 수 없습니다. 따라서 A 액터가 B 액터에 디펜던시를 가진다면 B 액터가 클라이언트에 생성될 때까지 기다려야 할 수 있습니다. 이러한 로직은 `TLiveData`의 대기 기능을 사용하면 아주 쉽게 구현할 수 있습니다.

```C++
UPROPERTY(ReplicatedUsing=OnRep_LastBoss)
ABossMonster* RepLastBoss;
TLiveData<ABossMonster*&> LastBoss{RepLastBoss};

UFUNCTION()
void OnRep_LastBoss() { LastBoss.Notify(); }
```

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    if (GetNetMode() == NM_Client)
    {
        RunWeakCoroutine(this, [this]() -> FWeakCoroutine
        {
            TAbortPtr<ABossMonster> Boss = co_await LastBoss;
            ShowMessage(TEXT("라스트 보스가 출현했습니다"));
            Boss->Roar();
        });
    }
}
```

서버에서는 라스트 보스 액터를 스폰한 후 해당 포인터를 `RepLastBoss` 변수에 세팅합니다. 클라이언트에서는 해당 포인터가 valid 해지는 타이밍 즉 클라이언트에 메세지가 도착해 액터가 스폰되는 시점에 OnRep 함수가 호출되며 `LastBoss`에게 포인터 도착을 알립니다. `LastBoss`의 타입을 자세히 보시면 포인터가 아니라 포인터에 대한 레퍼런스임을 알 수 있습니다. `LastBoss`가 내부에 데이터를 따로 유지하고 있는 것이 아니라 `RepLastBoss`의 데이터를 같이 사용하기 때문에 Cache Coherence에 대해서 신경 쓸 필요가 없습니다.

클라이언트에서는 `BeginPlay`가 호출된 이후 라스트 보스가 스폰되는 것을 기다렸다가 출현 메세지를 출력합니다. 특히 메세지나 `Roar`의 사운드 또는 애니메이션은 서버에서는 재생이 필요하지 않은 미디어이므로 if문을 사용해 제한합니다. 만약 `BeginPlay` 호출 시점에 이미 라스트 보스가 도착해있다면 바로 출현 메세지를 출력하므로 어느 경우에서든 로직이 망가지지 않습니다.

### `TValueStream`을 통한 비동기 데이터 스트림 관리

`TLiveData`에 앞으로 설정될 미래의 값들을 얻기 위해서는 `MakeStream` 함수를 호출해 `TValueStream`을 활용할 수 있습니다.

```C++
TLiveData<FString> Username;
```

```C++
virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        TValueStream<FStream> UsernameStream = Username.MakeStream();
        
        const FString Username0 = co_await UsernameStream;
        const FString Username1 = co_await UsernameStream;
        const FString Username2 = co_await UsernameStream;
    });
}
```

`TLiveData`에 새로운 값이 설정될 때마다 동일한 값이 스트림에 들어갑니다. 코루틴 내부에서는 스트림에 `co_await`를 호출해, 스트림에 이미 값이 들어있는 경우 즉시 꺼내거나 아직 아무 값도 들어있지 않은 경우 값이 도착할 때까지 기다린 후 꺼낼 수 있습니다.

### `Awaitables`를 통한 Awaitable의 조작

Weak Coroutine에서는 Awaitable의 뒤에 pipe operator를 추가하는 것으로 Awaitable의 반환 결과를 원하는 형태로 조작하는 것이 가능합니다. 가장 기본적인 함수는 `Awatiables::Transform`과 `Awaitables::Filter`로 C++20의 `views`와 유사한 방식으로 Awaitable의 결과를 변형(Trasnform)하거나 여과(Filter)할 수 있습니다.

다음은 가장 최근에 받은 대미지가 100 이상일 때만 대미지의 카테고리를 화면에 표시하는 예시입니다.

```C++
TLiveData<int32> DamageTaken;
```

```C++
FString DamageToDamageCategory(int32 Damage)
{
    // 각 대미지마다 적절한 FString을 반환한다...
}

virtual void BeginPlay() override
{
    Super::BeginPlay();

    RunWeakCoroutine(this, [this]() -> FWeakCoroutine
    {
        auto DamageCategoryStream
            = DamageTaken.MakeStream()
            | Awaitables::Filter([](int32 Damage) { return Damage >= 100; })
            | Awaitables::Transform(DamageToDamageCategory);
        
        while (true)
        {
            ShowDamageCategory(co_await DamageCategoryStream);
        }
    });
}
```

## Weak Coroutine의 활용 - 샘플 게임

샘플 게임에서 Weak Coroutine이 활용되는 모습은 글보다는 영상으로 보여드리는 편이 더 효과적으로 설명드릴 수 있기 때문에 아래 영상의 12분 10초부터 시청을 부탁드리겠습니다.

https://youtu.be/QxqItTpRW_c?si=o1I0f9C9g-zszknR&t=730

## Weak Coroutine의 구현

Weak Coroutine은 리포지토리의 `WeakCoroutine.h` 파일에 구현되어 있습니다. 이 중에서 중심이 되는 클래스를 나열하면 다음과 같습니다.

### `TWeakCoroutine`

Weak Coroutine의 `promise_type`에 해당하는 `TWeakCoroutinePromiseType`이 `return_object`로 반환하는 클래스입니다.

- `TAwaitableCoroutine`을 상속하기 때문에 이 `return_object`에 `co_await`를 걸어 `co_return`에 넘겨진 값을 반환 받는 것이 가능합니다. 
- `TAbortableCoroutine`을 상속해 `Abort` 기능을 구현합니다.

### `TWeakCoroutineProsmieType`

Weak Coroutine의 `prosmie_type`입니다.
- `TAwaitablePromise`을 상속하기 때문에 이 `return_object`에 `co_await`를 걸어 `co_return`에 넘겨진 값을 반환 받는 것이 가능합니다. 
- `TAbortablePromise`을 상속해 `Abort` 기능을 구현합니다.
- `TWeakPromise`를 상속해 위에서 설명드린 Weak List 기능을 구현합니다.
- `FLoggingPromise`를 상속해 코루틴이 suspend 상태에 들어갈 때 `std::source_location`을 기록해 어느 파일의 어느 라인에서 코루틴이 멈춰있는지 로그를 남기는 기능을 구현합니다.

Weak Coroutine에서 구현하는 여러가지 기능은 주로 `co_await`에 들어가거나 깨어날 때 발생합니다. 예를 들어서 에러 핸들링 기능이나 Weak List 체크 기능은 `co_await`에서 코루틴이 깨어날 때 실행됩니다. 이러한 기능은 `TWeakCoroutinePromiseType`의 `await_transform` 함수에 적절한 Awaitable들을 pipe operator로 chain한 형태로 구현되어 있습니다.

```C++
template <CAwaitableConvertible AnyType>
auto await_transform(AnyType&& Any)
{
    return await_transform(operator co_await(Forward<AnyType>(Any)));
}

template <CUObjectUnsafeWrapper UnsafeWrapperType>
auto await_transform(const UnsafeWrapperType& Wrapper)
{
    return Awaitables::AsAwaitable(Wrapper) | Awaitables::ReturnAsAbortPtr(*this);
}

template <typename WithErrorAwaitableType>
    requires TIsInstantiationOf_V<WithErrorAwaitableType, TCatchAwaitable>
auto await_transform(WithErrorAwaitableType&& Awaitable)
{
    using AllowedErrorTypeList = typename std::decay_t<WithErrorAwaitableType>::AllowedErrorTypeList;

    return Forward<WithErrorAwaitableType>(Awaitable).Awaitable
        | Awaitables::DestroyIfAbortRequested()
        | Awaitables::DestroyIfInvalidPromise()
        | Awaitables::DestroyIfErrorNotIn<AllowedErrorTypeList>()
        | Awaitables::ReturnAsAbortPtr(*this)
        | Awaitables::CaptureSourceLocation();
}

template <CAwaitable AwaitableType>
    requires !TIsInstantiationOf_V<AwaitableType, TCatchAwaitable>
auto await_transform(AwaitableType&& Awaitable)
{
    return Forward<AwaitableType>(Awaitable)
        | Awaitables::DestroyIfAbortRequested()
        | Awaitables::DestroyIfInvalidPromise()
        | Awaitables::DestroyIfError()
        | Awaitables::ReturnAsAbortPtr(*this)
        | Awaitables::CaptureSourceLocation();
}
```

즉 각 기능이 서로 독립적으로 구현되어 있기 때문에 원하는 함수의 정의를 따라가 각 기능이 어떻게 구현되어 있는지 확인할 수 있습니다. 예를 들어 `Abort` 호출 시 코루틴이 Abort하는 기능은 `Awaitables::DestroyIfAbortRequested()` 함수가 유저의 Awaitable을 자신의 Awaitable로 감싸 `co_await` 진입 시 또는 퇴장 시 코루틴을 재개하지 않고 파괴하는 형태로 구현되어 있습니다.

위 클래스들 및 함수를 시작으로 코드를 살펴보면 Weak Coroutine의 전체적인 구현을 파악하는 것이 가능합니다. 구현의 모든 부분을 전부 설명드릴 수는 없기 때문에 설명은 불가피하게 여기서 줄이게 되겠지만 만약 모호하거나 명료하지 않은 부분이 있다면 질문 주시면 성실하게 답변드리겠습니다.

## 결론

C++20에서 도입된 코루틴 기능은 이벤트 기반 프로그래밍에 있어서 코드 가독성을 높이고 프로그래머 효율을 증진하는 필수불가결한 도구입니다. 다만 언리얼 엔진은 UHT와 커스텀 GC라고 하는 독특한 코딩 환경을 가지고 있기 때문에 언리얼 엔진에 도입하기 위해서는 특별한 아이디어와 고려가 필요했습니다. Weak Coroutine은 C++20 코루틴을 언리얼 엔진의 실정에 맞추어 Weak List, `TAbortPtr`, `std::exception`을 사용하지 않는 에러 핸들링 기능(`TFailableResult`)을 구현함으로써 언리얼 엔진 고유의 환경에서 벗어나지 않으면서도 C++20의 새 기능을 자연스럽게 도입할 수 있었습니다. 간결하고 실수하기 어려운 인터페이스를 제공하는 Weak Coroutine은 프로그램의 버그를 줄이고 개발 시간을 단축하는 등의 효과를 낼 수 있을 것으로 기대하고 있습니다.
