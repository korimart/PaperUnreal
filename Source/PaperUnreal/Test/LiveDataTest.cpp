#include "CancellableFutureTest.h"
#include "Misc/AutomationTest.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveDataTest, "PaperUnreal.PaperUnreal.Test.LiveDataTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveDataTest::RunTest(const FString& Parameters)
{
	{
		TLiveData<int32> LiveData;

		TArray<int32> Received;
		auto Handle = LiveData.Observe([&](int32 Value)
		{
			Received.Add(Value);
		});

		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 1);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 0);
		LiveData = 0;
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 1);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 0);
		LiveData = 1;
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 2);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 1);
		LiveData = 5;
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 3);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 5);
		LiveData.SetValue(5);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 3);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 5);
		LiveData.SetValueNoComparison(5);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 4);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 5);
		LiveData.Modify([&](int32& Value)
		{
			Value = 6;
			return true;
		});
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Num(), 5);
		TestEqual(TEXT("non validable type인 live data의 observe 테스트"), Received.Last(), 6);
	}

	{
		TLiveData<int32> LiveData;

		TArray<int32> Received;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			for (auto Stream = LiveData.MakeStream();;)
			{
				Received.Add(co_await Stream);
			}
		});

		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 1);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 0);
		LiveData = 0;
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 1);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 0);
		LiveData = 1;
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 2);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 1);
		LiveData = 5;
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 3);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 5);
		LiveData.SetValue(5);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 3);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 5);
		LiveData.SetValueNoComparison(5);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 4);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 5);
		LiveData.Modify([&](int32& Value)
		{
			Value = 6;
			return true;
		});
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Num(), 5);
		TestEqual(TEXT("non validable type인 live data의 MakeStream 테스트"), Received.Last(), 6);
	}

	{
		// 레퍼런스 테스트인데 귀찮아서 컴파일 되는지만 테스트
		int32 BackingField;
		TLiveData<int32&> LiveData{BackingField};
		auto Handle = LiveData.Observe([](auto)
		{
		});
		LiveData.MakeStream();
		LiveData.SetValue(5);
	}

	{
		TLiveData<TOptional<int32>> LiveData;

		TArray<TOptional<int32>> Received;
		auto Handle = LiveData.Observe([&](TOptional<int32> Value)
		{
			Received.Add(Value);
		});

		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 1);
		TestFalse(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last().IsSet());
		LiveData = 0;
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 2);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), *Received.Last(), 0);
		LiveData = 1;
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 3);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), *Received.Last(), 1);
		LiveData = 5;
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 4);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), *Received.Last(), 5);
		LiveData.SetValue(5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 4);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), *Received.Last(), 5);
		LiveData.SetValueNoComparison(5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), *Received.Last(), 5);
		LiveData.Modify([&](TOptional<int32>& Value)
		{
			Value = 6;
			return true;
		});
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 6);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), *Received.Last(), 6);
		LiveData = TOptional<int32>{};
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 7);
		TestFalse(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last().IsSet());
	}

	{
		TLiveData<TOptional<int32>> LiveData;

		TArray<int32> Received;
		auto Handle = LiveData.ObserveIfValid([&](int32 Value)
		{
			Received.Add(Value);
		});

		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 0);
		LiveData = 0;
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 1);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 0);
		LiveData = 1;
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 2);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 1);
		LiveData = 5;
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 3);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 5);
		LiveData.SetValue(5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 3);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 5);
		LiveData.SetValueNoComparison(5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 4);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 5);
		LiveData.Modify([&](TOptional<int32>& Value)
		{
			Value = 6;
			return true;
		});
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 6);
		LiveData = TOptional<int32>{};
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Num(), 5);
		TestEqual(TEXT("validable type(TOptional)인 live data의 observe 테스트"), Received.Last(), 6);
	}

	{
		TArray<UDummy*> BackingField;
		TLiveData<TArray<UDummy*>&> LiveData{BackingField};

		// 컴파일 되는지 테스트
		LiveData.CreateAddStream();
		LiveData.CreateStrictAddStream();

		TArray<UDummy*> Added;
		auto Handle = LiveData.ObserveAddIfValid([&](UDummy* Value)
		{
			Added.Add(Value);
		});

		TArray<UDummy*> Removed;
		auto Handle2 = LiveData.ObserveRemoveIfValid([&](UDummy* Value)
		{
			Removed.Add(Value);
		});

		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 0);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 0);
		LiveData.Add(nullptr);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), BackingField.Num(), 1);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 0);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 0);
		LiveData.AddIfValid(nullptr);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), BackingField.Num(), 1);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 0);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 0);
		LiveData.AddIfValid(NewObject<UDummy>());
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 1);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 0);
		LiveData.AddIfValid(NewObject<UDummy>());
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 2);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 0);
		LiveData.Add(NewObject<UDummy>());
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 3);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 0);
		LiveData.RemoveAt(1);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 3);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 1);
		LiveData.RemoveAt(1);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 3);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 2);

		UDummy* Garbage = NewObject<UDummy>();
		Garbage->MarkAsGarbage();
		LiveData.Add(Garbage);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 3);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 2);
		LiveData.Remove(Garbage);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Added.Num(), 3);
		TestEqual(TEXT("validable type을 element로 가지는 TArray의 Observe 테스트"), Removed.Num(), 2);
	}

	{
		TLiveData<TArray<int32>> LiveData;

		TArray<int32> Received;
		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			for (auto Stream = LiveData.CreateStrictAddStream();;)
			{
				Received.Add(co_await Stream);
			}
		});

		// 구현 디테일 중에 Delegate의 복사가 발생하면 안 되도록 땜빵한 게 있어서
		// 멀티캐스트 델리게이트의 내부 Delegate Array가 커질 때 복사 안 하는지 테스트
		for (int32 i = 0; i < 50; i++)
		{
			LiveData.CreateStrictAddStream();
		}

		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 0);
		LiveData.Add(0);
		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 1);
		LiveData.Add(0);
		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 2);
		LiveData.Add(0);
		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 3);
		LiveData.Remove(0);
		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 3);
		LiveData.Add(0);
		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 3);
		LiveData.Add(0);
		TestEqual(TEXT("TArray의 create strict add stream 테스트"), Received.Num(), 3);
	}

	{
		TLiveData<int32> LiveData;

		TestEqual(TEXT("AwaitAndModify가 Guard를 기다렸다가 잘 실행되는지 테스트"), LiveData.Get(), 0);

		auto Handle = LiveData.Observe([&](int32 Value)
		{
			if (Value >= 100)
			{
				return;
			}

			LiveData.AwaitAndModify([](int32& Value)
			{
				Value++;
				return true;
			});

			LiveData.AwaitAndModify([](int32& Value)
			{
				Value++;
				return true;
			});

			LiveData.AwaitAndModify([](int32& Value)
			{
				Value++;
				return true;
			});
		});

		// 한 번 Observe가 실행될 때마다 AwaitAndModify가 3개 예약됨
		// 이걸 100번 예약하고 예약된 걸 전부다 flush하면 값이 300이 되어야 함
		// 공식 = 3n (n은 observe를 호출한 횟수)
		TestEqual(TEXT("AwaitAndModify가 Guard를 기다렸다가 잘 실행되는지 테스트"), LiveData.Get(), 300);
	}

	{
		TLiveData<int32> LiveData;
		LiveData.Modify([&](int32& Value)
		{
			TestEqual(TEXT("AwaitAndModify가 Guard를 기다렸다가 잘 실행되는지 테스트"), Value, 0);

			LiveData.AwaitAndModify([](int32& Value)
			{
				Value++;
				return true;
			});
			TestEqual(TEXT("AwaitAndModify가 Guard를 기다렸다가 잘 실행되는지 테스트"), Value, 0);

			LiveData.AwaitAndModify([](int32& Value)
			{
				Value++;
				return true;
			});
			TestEqual(TEXT("AwaitAndModify가 Guard를 기다렸다가 잘 실행되는지 테스트"), Value, 0);

			LiveData.AwaitAndModify([](int32& Value)
			{
				Value++;
				return true;
			});
			TestEqual(TEXT("AwaitAndModify가 Guard를 기다렸다가 잘 실행되는지 테스트"), Value, 0);

			return true;
		});

		TestEqual(TEXT(""), LiveData.Get(), 3);
	}

	{
		TLiveData<TArray<int32>> LiveData;
		int32 LoopCount = 0;

		RunWeakCoroutine([&]() -> FWeakCoroutine
		{
			while (LoopCount < 100)
			{
				co_await LiveData.CreateStrictAddStream().EndOfStream();
				LoopCount++;
			}
		});

		LiveData.Add(42);
		LiveData.Add(42);
		LiveData.Add(42);
		LiveData.Add(99);
		LiveData.Add(99);
		LiveData.Add(99);
		LiveData.Add(99);
		LiveData.Add(99);
		TestEqual(TEXT("LiveData의 remove 함수들이 적절한 타이밍에 strict add stream을 종료하는지 테스트"), LoopCount, 0);
		LiveData.RemoveAt(0);
		TestEqual(TEXT("LiveData의 remove 함수들이 적절한 타이밍에 strict add stream을 종료하는지 테스트"), LoopCount, 1);
		LiveData.Remove(42);
		TestEqual(TEXT("LiveData의 remove 함수들이 적절한 타이밍에 strict add stream을 종료하는지 테스트"), LoopCount, 2);
		LiveData.Empty();
		TestEqual(TEXT("LiveData의 remove 함수들이 적절한 타이밍에 strict add stream을 종료하는지 테스트"), LoopCount, 3);

		LiveData.Add(42);
		LiveData.Add(42);
		LiveData.Add(42);
		LiveData.Add(99);
		LiveData.Add(99);
		TArray<int32> Old = LiveData.Get();
		LiveData.SetValueSilent(TArray<int32>{});
		TestEqual(TEXT("LiveData의 remove 함수들이 적절한 타이밍에 strict add stream을 종료하는지 테스트"), LoopCount, 3);
		LiveData.NotifyDiff(Old);
		TestEqual(TEXT("LiveData의 remove 함수들이 적절한 타이밍에 strict add stream을 종료하는지 테스트"), LoopCount, 4);
	}

	return true;
}
