// Fill out your copyright notice in the Description page of Project Settings.

#include "MainMenu/MyMainMenuGameMode.h"
// 引入即将编写的主菜单 UI
#include "MainMenu/UI/MyMainMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"

// 💥【修改说明】引入存档大管家，以便在主菜单启动时预热内存台账
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "Game/MyGameInstance.h"


// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMyMainMenuGameMode::AMyMainMenuGameMode()
{
	// 主菜单是纯 2D 静态场景，不需要执行无缝漫游
	bUseSeamlessTravel = false;

	// 主菜单不需要生成任何默认的玩家肉体
	DefaultPawnClass = nullptr;
}

void AMyMainMenuGameMode::BeginPlay()
{
	// 必须调用父类，完成世界底层构建
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	// 💥【修改说明】主菜单必须在第一时间唤醒大管家预读硬盘注册表。否则存档 UI 找不到台账内存镜像，会触发防御机制直接阻断所有渲染与翻页指令。
	// 通过 World->GetGameInstance 强制转换为 UMyGameInstance 后才能正常获取您的专属子系统。
	if (UMyGameInstance* MyGI = World->GetGameInstance<UMyGameInstance>())
	{
		if (UMySaveSubsystem* SaveSub = MyGI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->PreloadRegistry();
		}
	}

	// 1. 抓取引擎底层自动分配的默认空壳玩家控制器
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		// 强行显示鼠标光标
		PC->bShowMouseCursor = true;

		// 强制将输入模式锁定为【纯 UI 模式】，彻底剥夺底层的游戏空间交互
		FInputModeUIOnly InputMode;
		// 允许鼠标划出窗口边缘（双屏玩家体验更好）
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	// 2. 实例化并拉起主菜单 UI
	if (MainMenuWidgetClass)
	{
		if (UMyMainMenuWidget* MainMenu = CreateWidget<UMyMainMenuWidget>(World, MainMenuWidgetClass))
		{
			// 将主菜单添加至屏幕，ZOrder 设为 0 (基础底板)
			MainMenu->AddToViewport(0);
		}
	}
}

#pragma endregion