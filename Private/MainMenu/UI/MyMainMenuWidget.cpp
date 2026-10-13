// Fill out your copyright notice in the Description page of Project Settings.

#include "MainMenu/UI/MyMainMenuWidget.h"
#include "UI/CommonButton/MyCommonButtonBase.h"
#include "UI/SaveGame/MySaveMenuWidget.h"
#include "MainMenu/MyMainMenuGameMode.h"
#include "Game/MyGameInstance.h"
#include "MapTravel/MyMapTravelSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"


// ==============================================================================
// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
// ==============================================================================
#pragma region

void UMyMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 动态绑定三个核心按钮的点击事件
	if (Btn_StartGame) Btn_StartGame->OnClicked().AddUObject(this, &UMyMainMenuWidget::OnStartGameClicked);
	if (Btn_LoadGame) Btn_LoadGame->OnClicked().AddUObject(this, &UMyMainMenuWidget::OnLoadGameClicked);
	if (Btn_QuitGame) Btn_QuitGame->OnClicked().AddUObject(this, &UMyMainMenuWidget::OnQuitGameClicked);
}

void UMyMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 焦点引导：面板出现时，强行把输入焦点锁定在“开始游戏”按钮上
	// 完美解决手柄和纯键盘玩家在主菜单无法上下导航的死锁问题
	if (Btn_StartGame)
	{
		Btn_StartGame->SetFocus();
	}
}

#pragma endregion


// ==============================================================================
// 交互逻辑处理 (Interaction Logic)
// ==============================================================================
#pragma region

void UMyMainMenuWidget::OnStartGameClicked()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 1. 获取目标地图
	AMyMainMenuGameMode* GM = Cast<AMyMainMenuGameMode>(World->GetAuthGameMode());
	if (!GM || GM->DefaultStartMap.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[主菜单] 致命错误：GameMode 中未配置 DefaultStartMap，无法发车！"));
		return;
	}

	// 2. 极致大扫除：确保这是纯净开荒，彻底清理大管家手里的前朝余孽
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		// 释放所有过往记录与肉体缓存
		GI->ClearVisitedMaps();
		GI->PlayerClassMemory.Empty();
		GI->SquadClassMemory.Empty();

		// 重置跨界锁，确保状态机为白纸
		GI->ResetTransitionLocks();
	}

	// 3. 提取纯净短名并呼叫 3A 漫游管线发车
	FName TargetMapName = FName(*GM->DefaultStartMap.GetAssetName());

	if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
	{
		UE_LOG(LogTemp, Warning, TEXT("🚀 [主菜单] 开荒起航！目标世界: %s"), *TargetMapName.ToString());

		// 底层会自动拉起黑幕，执行 ServerTravel，剥夺操作，完成极其平滑的跨界！
		TravelSub->ExecuteMapTravel(TargetMapName);
	}
}

void UMyMainMenuWidget::OnLoadGameClicked()
{
	// 采用同步保底防线加载 UI 资产 (防手速过快)
	if (!SaveMenuInstance && !SaveMenuClass.IsNull())
	{
		if (UClass* LoadedClass = SaveMenuClass.LoadSynchronous())
		{
			SaveMenuInstance = CreateWidget<UMySaveMenuWidget>(GetOwningPlayer(), LoadedClass);

			if (SaveMenuInstance)
			{
				// 【只读改造】：强制将此面板切为只读模式！
				// 💥【修改说明】将加锁指令提前至 AddToViewport（底层触发 NativeConstruct）之前，确保底层双对象池第一次注水时就能拿到只读指令，彻底屏蔽新建存档按钮。
				SaveMenuInstance->bIsLoadOnlyMode = true;

				// ZOrder 设为 10，确保盖在主菜单上方
				SaveMenuInstance->AddToViewport(10);
			}
		}
	}
	else if (SaveMenuInstance)
	{
		// 💥【修改说明】UI 被隐藏复用时，确保每次打开都重新锁定为只读。
		SaveMenuInstance->bIsLoadOnlyMode = true;
	}

	if (SaveMenuInstance)
	{
		// 呼叫 CommonUI 的 ActivateWidget。
		// 它会自动抢夺焦点、压入你写的 UIManagerSubsystem，并完美播放入场动画。主菜单无需做任何事。
		SaveMenuInstance->ActivateWidget();
	}
}

void UMyMainMenuWidget::OnQuitGameClicked()
{
	// 直接呼叫虚幻引擎底层的退出管线
	// 参数说明：当前世界，本地玩家控制器，退出偏好，是否忽略平台限制
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}

#pragma endregion