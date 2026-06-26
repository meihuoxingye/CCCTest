// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/UI/MyUIHandlerComponent.h"
#include "UI/MyMainHUDWidget.h"
#include "Game/MyGameModeBase.h"
#include "Character/TopCharacter.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
// 【新增】引入异步加载与系统管理头文件
#include "TimerManager.h"
#include "Engine/AssetManager.h"
// 引入控制器
#include "Character/TopPlayerController.h"

UMyUIHandlerComponent::UMyUIHandlerComponent()
{
	// 组件在逻辑上没有每帧更新的需求
	PrimaryComponentTick.bCanEverTick = false;
}

// ==============================================================================
// UI 统筹系统 (UI Management System)
// ==============================================================================
#pragma region

void UMyUIHandlerComponent::BeginPlay()
{
	Super::BeginPlay();

	// 【进阶优化】：在 BeginPlay 阶段即刻获取并锁定控制器指针
	// 避免后续在每一帧或每次按键高频交互时进行昂贵的 Cast 转换
	CachedPC = Cast<ATopPlayerController>(GetOwner());
	if (!CachedPC) return;

	// 【架构核心】：主动监听 GameMode 的名册更新频道，实现真正的事件驱动
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		// 无论何时，只要有人调用了 Broadcast，底层的事件系统会自动唤醒这里的 UpdateHUD 进行刷新
		GM->OnRosterChanged.AddUniqueDynamic(this, &UMyUIHandlerComponent::UpdateHUD);
	}

	// HUD 控件的初始化与首帧构建
	if (MainHUDClass)
	{
		// 创造主 UI 组件，拥有者为当前玩家控制器
		MainHUDInstance = CreateWidget<UMyMainHUDWidget>(CachedPC, MainHUDClass);

		if (MainHUDInstance)
		{
			// 将主 UI 组件添加到视口
			MainHUDInstance->AddToViewport();
			// 更新 UI 界面
			UpdateHUD();
		}
	}
}

// ==============================================================================
// 工业级：时间分片后台预热器
// ==============================================================================
void UMyUIHandlerComponent::ProcessNextWarmup()
{
	// 【防线 1：宿主有效性校验】
	// 前置检查玩家控制器缓存是否有效，防止角色突然死亡或关卡切换导致的野指针崩溃
	if (!CachedPC) return;

	// 第一步：预热战术面板 (开局第 2 秒触发)
	// 【状态机步进管理】：利用 CurrentWarmupStep 实现“时间切片”，确保开局的繁重加载任务被分散到不同的时间点，避免同一帧扎堆导致游戏卡顿。
	if (CurrentWarmupStep == 0)
	{
		CurrentWarmupStep++; // 推进状态，保证该步骤（第一步）终其一生只会被执行一次

		// 【防线 2：资产配置校验】
		// 检查蓝图中的软类引用 (SoftClassPtr) 是否为空，防止去硬盘里加载一个“虚无”。
		if (!TacticalWidgetClass.IsNull())
		{
			// 获取虚幻引擎全局的“异步流式加载大管家”
			FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

			// 【异步 IO 释放主线程】：
			// 向大管家提交软路径，让后台线程去硬盘里把巨大的 UI 资产（包含材质、贴图）慢慢读进内存。
			// 此时主线程 (游戏画面) 继续丝滑运行。等读完了，才会回头调用后面的 Lambda 匿名函数。
			Streamable.RequestAsyncLoad(TacticalWidgetClass.ToSoftObjectPath(), [this]()
				{
					// 【防抢占锁】：如果玩家手速极快，在 2 秒内已经按 Tab 触发了同步保底加载，
					// 那么实例已存在，绝不能覆盖指针！直接跳过本次创建。
					// （高级架构：完美化解了异步回调极易引发的“业务竞态条件”导致的内存泄漏与指针覆盖）
					if (!TacticalWidgetInstance)
					{
						// 安全地将内存中已就绪的 UClass 提取出来
						if (UClass* LoadedClass = TacticalWidgetClass.Get())
						{
							// 实例化这个重度战术面板 UI
							TacticalWidgetInstance = CreateWidget<UMyActivatableWidgetBase>(CachedPC, LoadedClass);
							if (TacticalWidgetInstance)
							{
								// 【预热核心 1：底层静默挂载】
								// 强行塞入屏幕，ZOrder 设为极小的负数 (-999)，保证它躲在所有画面的最底层，绝不遮挡视野。
								TacticalWidgetInstance->AddToViewport(-999);

								// 【预热核心 2：Slate 引擎排版欺骗】
								// 为什么不用 Collapsed？
								// 因为 Hidden 状态下，UI 看不见，但 Slate 引擎依然会老老实实地为它构建控件树、计算长宽尺寸、编译材质着色器（非常耗时）！
								// 这等同于提前“白嫖”了第一次打开面板时最卡顿的排版时间！
								TacticalWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

								// 将 UI 内部的关闭请求，接回本组件的事件网络中
								TacticalWidgetInstance->OnCloseRequested.AddUniqueDynamic(this, &UMyUIHandlerComponent::HandleWidgetCloseRequested);

								// 【预热核心 3：次帧收尾 (NextTick)】
								// 为什么要等下一帧？
								// 因为上面刚设为 Hidden，必须让引擎跑完当前这一帧的渲染管线，尺寸才算彻底计算完毕。
								GetWorld()->GetTimerManager().SetTimerForNextTick([this]() {
									// 等引擎老老实实算完尺寸后，立刻将它设为 Collapsed（彻底折叠）！
									// 意义：Collapsed 状态下的 UI 彻底不参与引擎每帧的 Layout 遍历，将潜伏期的 CPU 性能损耗降为绝对的 0！
									if (TacticalWidgetInstance && !bIsTacticalUIOpen) TacticalWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
									});
							}
						}
					}

					// 当前面板渲染预热搞定后，再让 CPU 休息 1.5 秒，然后触发下一个预热！
					// 【削峰填谷】：主动留白 1.5 秒，让开局时其他的运算任务（如刷怪、AI寻路初始化）有喘息之机，这是非常成熟的 3A 性能分配策略。
					FTimerHandle NextTimer;
					GetWorld()->GetTimerManager().SetTimer(NextTimer, this, &UMyUIHandlerComponent::ProcessNextWarmup, 1.5f, false);
				});
		}
		else { ProcessNextWarmup(); } // 跳过空项，立刻进行下一步预热（尾递归）
	}


	// 第二步：预热存档面板 (开局第 3.5 秒触发)
	else if (CurrentWarmupStep == 1)
	{
		CurrentWarmupStep++; // 推进状态，保证该步骤终其一生只会被执行一次

		// 检查蓝图中的软类引用是否为空
		if (!SaveMenuClass.IsNull())
		{
			// 获取虚幻引擎全局的“异步流式加载大管家”
			FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

			// 向后台线程提交异步加载请求
			Streamable.RequestAsyncLoad(SaveMenuClass.ToSoftObjectPath(), [this]()
				{
					// 【防抢占锁】：如果玩家手速极快，在开局 3.5 秒内已经摸了存档点，
					// 触发了同步保底加载，此时实例已存在，绝不能覆盖指针！直接跳过。
					if (!SaveMenuInstance)
					{
						// 安全地将内存中已就绪的 UClass 提取出来
						if (UClass* LoadedClass = SaveMenuClass.Get())
						{
							// 实例化存档面板 UI
							SaveMenuInstance = CreateWidget<UMyActivatableWidgetBase>(CachedPC, LoadedClass);
							if (SaveMenuInstance)
							{
								// 【预热核心 1：底层静默挂载】
								// 强行塞入屏幕，ZOrder 设为极小的负数 (-999)，保证绝不遮挡视野
								SaveMenuInstance->AddToViewport(-999);

								// 【预热核心 2：Slate 引擎排版欺骗】
								// 设置为 Hidden，诱导 Slate 引擎在后台默默计算它的长宽尺寸并编译材质
								SaveMenuInstance->SetVisibility(ESlateVisibility::Hidden);

								// 【事件管线绑定】
								// 将 UI 内部发出的关闭请求，绑定到大管家专属的存档面板关闭函数上
								SaveMenuInstance->OnCloseRequested.AddUniqueDynamic(this, &UMyUIHandlerComponent::HandleSaveMenuCloseRequested);

								// 【预热核心 3：次帧收尾 (NextTick)】
								GetWorld()->GetTimerManager().SetTimerForNextTick([this]() {
									// 等引擎乖乖算完尺寸后，立刻设为 Collapsed，将其渲染和排版开销降为 0！
									if (SaveMenuInstance && !bIsSaveMenuOpen)
									{
										SaveMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
									}
									});
							}
						}
					}

					// 【架构拓展性】：如果未来你还要做“技能树面板”、“全屏地图”，
					// 就让 CPU 继续休息 1.5 秒后触发第三步。
					// 如果没有第三步了，下一次调用 ProcessNextWarmup 时会自动 Return 退出。
					FTimerHandle NextTimer;
					GetWorld()->GetTimerManager().SetTimer(NextTimer, this, &UMyUIHandlerComponent::ProcessNextWarmup, 1.5f, false);
				});
		}
		else
		{
			// 如果软引用为空（没配置类），跳过当前项，立刻尾递归进入下一步预热
			ProcessNextWarmup();
		}
	}
}

void UMyUIHandlerComponent::UpdateHUD()
{
	// 如果 UI 面板还未就绪，或者不在游戏世界里，直接返回
	if (!MainHUDInstance || !GetWorld()) return;

	// O(1) 极速调取：向当前 GameMode 索要已过滤好、最干净的存活友军名单
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		// 【进阶优化】：直接使用预先缓存的 CachedPC 提取 Pawn，消灭 Cast<APlayerController>(GetOwner())
		ATopCharacter* MyPawn = CachedPC ? Cast<ATopCharacter>(CachedPC->GetPawn()) : nullptr;
		// 传递名单，并将当前玩家控制器所附身的 Pawn 转化为 ATopCharacter 作为当前活跃单位传入
		MainHUDInstance->UpdateSquadList(GM->FriendlyRoster, MyPawn);
	}
}

#pragma endregion

// ==============================================================================
// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
// ==============================================================================
#pragma region


void UMyUIHandlerComponent::ToggleTacticalWidget()
{
	ToggleTacticalWidget(!bIsTacticalUIOpen);
}

void UMyUIHandlerComponent::ToggleTacticalWidget(bool bShouldOpen)
{
	// 使用 O(1) 缓存的控制器指针进行拦截
	if (!CachedPC) return;

	// 【进阶优化：状态原子性锁】：如果目标状态与当前状态完全一致，则直接掐断逻辑！
	// 这从根本上杜绝了网络延迟或极短时间内重复触发导致的 UI 重复压栈和状态撕裂
	if (bIsTacticalUIOpen == bShouldOpen) return;

	// 懒加载模式：如果战术 UI 实例还未创建，且蓝图里配置了具体的类模板，则开始创建
	// 【适配预热保底】：即使玩家在开局前 2 秒预热没跑完时就按了键，使用 LoadSynchronous() 保底瞬间加载
	if (!TacticalWidgetInstance && !TacticalWidgetClass.IsNull())
	{
		// 在当前控制器的内存名下创建这个战术 UI 蓝图的实例
		if (UClass* LoadedClass = TacticalWidgetClass.LoadSynchronous())
		{
			TacticalWidgetInstance = CreateWidget<UMyActivatableWidgetBase>(CachedPC, LoadedClass);

			// 确保 UI 实例创建成功
			if (TacticalWidgetInstance)
			{
				// 添加到屏幕视口，ZOrder 设置为 100 保证它盖在游戏画面和其他普通 UI 之上
				// ZOrder（Z 轴排序/层级），数字越大，这个 UI 所在的层就越靠前，会遮挡住底下的 UI
				TacticalWidgetInstance->AddToViewport(100);
				// 初始创建时强制设为隐藏（Collapsed），防止在还没播放动画时出现一帧的画面闪烁
				TacticalWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);

				// 绑定到当前组件特制的转发函数，用于通知控制器！
				TacticalWidgetInstance->OnCloseRequested.AddUniqueDynamic(this, &UMyUIHandlerComponent::HandleWidgetCloseRequested);
			}
		}
	}

	// 安全校验：如果 UI 实例依然为空（比如粗心没配置类模板），直接退出防止引发野指针崩溃
	if (!TacticalWidgetInstance) return;

	// 获取增强输入系统的本地玩家子系统，用于后续插拔输入映射上下文 (IMC)
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(CachedPC->GetLocalPlayer());

	// 【安全应用明确的目标状态】
	bIsTacticalUIOpen = bShouldOpen;

	// 如果判定为：准备打开战术面板
	if (bIsTacticalUIOpen)
	{
		// 【必须加回来】：赋予 UI 物理体积，打破 CommonUI 对 Collapsed 控件的死锁拦截！
		TacticalWidgetInstance->SetVisibility(ESlateVisibility::Visible);

		// 呼叫 UI 实例执行 CommonUI 的“被激活”逻辑，它会自动触发 UMyActivatableWidgetBase 的 NativeOnActivated
		// NativeOnActivated 核心职责：将状态切为 Opening，强制底层立刻排版（防闪烁），并将自身推入子系统拦截栈接管焦点。
		TacticalWidgetInstance->ActivateWidget();

		// 为增强输入系统挂载战术面板专用的 IMC，优先级设为 10（高于默认的 0）
		// IMC（输入映射上下文）是可以像穿衣服一样一层一层“穿上”和“脱下”的
		// 若此 IMC 的优先级更高，则会覆盖掉底层的开火、移动等操作
		if (Subsystem && TacticalIMC) Subsystem->AddMappingContext(TacticalIMC, 10);
	}

	// 如果判定为：准备关闭战术面板
	else
	{
		// 呼叫 UI 实例执行它自己的“反激活”逻辑，它会自动触发 UMyActivatableWidgetBase 的 NativeOnDeactivated
		// NativeOnDeactivated 核心职责：将状态切为 Closing ，在 NativeTick 里触发退场动画。注意：为防幽灵点击，此处仅改状态，绝不出栈。
		TacticalWidgetInstance->DeactivateWidget();

		// 从输入系统中剥夺战术面板专属的 IMC，把按键映射还给正常的 3D 游戏操作
		// RemoveMappingContext 卸载 IMC
		if (Subsystem && TacticalIMC) Subsystem->RemoveMappingContext(TacticalIMC);

		// 强制将引擎底层的“输入焦点”从 UI 身上剥离，并交还给 3D 游戏视口。这彻底解决了 UI 关闭后第一下鼠标点击无效的问题
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UMyUIHandlerComponent::HandleWidgetCloseRequested()
{
	// 【进阶优化】：通过 BeginPlay 中预先缓存好的 CachedPC 进行 O(1) 极速调用，彻底消除 Cast 类型转换开销
	// 【逻辑还原】：当 UI 内部（如点击背景、按取消键）发来关闭请求时
	// 向上汇报给控制器大管家，让控制器去同时关闭 UI + 退出子弹时间！
	if (CachedPC)
	{
		CachedPC->ToggleTacticalMode();
	}
}

void UMyUIHandlerComponent::ToggleSaveMenuWidget()
{
	ToggleSaveMenuWidget(!bIsSaveMenuOpen);
}

void UMyUIHandlerComponent::ToggleSaveMenuWidget(bool bShouldOpen)
{
	// 【防线】：宿主有效性校验，防止角色死亡后依然触发 UI 逻辑导致空指针崩溃
	if (!CachedPC) return;

	// 状态原子性锁：杜绝重复触发
	// 【架构意义】：防止玩家在 0.01 秒内连按两次打开/关闭键，导致 UI 状态机（入场/退场动画）发生重入冲突或死锁
	if (bIsSaveMenuOpen == bShouldOpen) return;

	// ==============================================================================
	// 懒加载模式与同步保底防线 (Lazy Loading & Synchronous Fallback)
	// ==============================================================================
	// 【适配预热保底】：如果玩家在后台 3.5 秒预热完成前就跑去点了存档点，提供强制同步加载保底
	if (!SaveMenuInstance && !SaveMenuClass.IsNull())
	{
		// 【防线突破妥协】：玩家手速超过了异步预热速度。强行挂起主线程（引发微弱卡顿），
		// 逼迫操作系统立刻去硬盘同步拉取 UI 资产，确保核心业务（存档）绝不因为资源未就绪而崩溃。
		if (UClass* LoadedClass = SaveMenuClass.LoadSynchronous())
		{
			SaveMenuInstance = CreateWidget<UMyActivatableWidgetBase>(CachedPC, LoadedClass);
			if (SaveMenuInstance)
			{
				// ZOrder 为 10：确立存档面板极高的物理覆盖层级
				SaveMenuInstance->AddToViewport(10);
				// 设置为 Collapsed 只建仓不渲染，把展开的表演权移交给后续的 CommonUI 原生管线 ActivateWidget()
				SaveMenuInstance->SetVisibility(ESlateVisibility::Collapsed);

				// 【事件管线热插拔与控制反转】
				// 将 UI 内部发出的“关闭请求”信号（例如玩家点击了面板上的返回按钮，或按下了 Esc 键），
				// 向上接入本组件的专属处理函数 (HandleSaveMenuCloseRequested) 中。
				// @架构意义：UI 绝不自裁！UI 只负责发信号，由统筹组件负责执行真正的关闭、出栈以及鼠标焦点的重置。
				// @安全机制：使用 AddUniqueDynamic 而非 AddDynamic，提供绝对防线，确保即使在极短时间内多次触发保底建仓逻辑，
				// 该委托也只会被绑定一次，彻底根绝关闭事件被重复触发导致的状态机崩溃。
				SaveMenuInstance->OnCloseRequested.AddUniqueDynamic(this, &UMyUIHandlerComponent::HandleSaveMenuCloseRequested);
			}
		}
	}

	if (!SaveMenuInstance) return;

	// 应用状态
	bIsSaveMenuOpen = bShouldOpen;

	if (bIsSaveMenuOpen)
	{
		// 【必须加回来】：赋予 UI 物理体积，打破 CommonUI 对 Collapsed 控件的死锁拦截！
		SaveMenuInstance->SetVisibility(ESlateVisibility::Visible);

		// 发送点火信号，呼叫 CommonUI 总司令全自动推流、入栈
		// 【移交大权】：调用原生 ActivateWidget() 后，CommonUI 底层会接管一切：
		// 自动分配焦点、拦截输入，并在最后一刻触发 NativeOnActivated 开启你的双轨入场动画。
		SaveMenuInstance->ActivateWidget();

		// ==============================================================================
		// 【核武级修复】：彻底清除“幽灵输入/按键粘滞” (Fix Input Ghosting)
		// ==============================================================================
		// 灾难重现：玩家按住 'W' 或 'D'（奔跑）时，突然按 E 打开了存档面板。
		// 由于 UI 瞬间抢走了引擎的“输入焦点”，键盘松开时的 KeyUp 事件被 UI 护盾挡住了！
		// 底层 PlayerController 永远收不到“按键已抬起”的信号，导致主角一直在背景里撞墙奔跑（按键粘滞）。
		if (CachedPC)
		{
			// 1. 强令底层的 PlayerController 清空所有物理按键的“按下”状态
			// 这会强行给增强输入系统发送 Cancel/Completed 信号，斩断正在执行的移动动作
			CachedPC->FlushPressedKeys();

			// 2. （绝对兜底）清空角色当前帧残余的移动方向向量，实现瞬间急刹车
			// 物理层面彻底粉碎角色身上的惯性残留，确保打开 UI 的瞬间主角乖乖立正。
			if (APawn* MyPawn = CachedPC->GetPawn())
			{
				MyPawn->ConsumeMovementInputVector();
			}
		}
	}
	else
	{
		// 触发收起信号，呼叫 CommonUI 总司令执行退场连招
		// 【防幽灵点击】：调用原生 DeactivateWidget() 会瞬间剥夺 UI 的输入焦点，
		// 但不会立刻销毁面板。它会触发 NativeOnDeactivated 将状态切为 Closing，
		// 继续用 UI 挡住屏幕防走火，直到 Tick 里的退场动画彻底播完，基类才会自己出栈。
		SaveMenuInstance->DeactivateWidget();

		// =====================================================================
		// 【终极修复：消除存档面板的双击 Bug (Focus Trap)】
		// 1. 补上战术面板同款的“上帝之手”，强制剥夺 UI 的输入焦点
		// =====================================================================
		// 灾难重现：UI 虽然关了（或正在播放退场动画），但操作系统的焦点还残留在 UI 树上。
		// 此时玩家点左键开火，第一枪会被系统判定为“点回游戏窗口（夺回焦点）”，第二枪才能打出去。
		// 解决方案：调用 Slate 大管家，发动上帝之手，强行把全局焦点从 UI 树上扒下来，直接砸回底层的 3D 游戏视口！保证关 UI 后第一枪绝对能射出。
		FSlateApplication::Get().SetAllUserFocusToGameViewport();

		// =====================================================================
		// 2. 强行冲刷底层的输入状态机 (Input Pipeline Flush)
		// =====================================================================
		// 灾难重现：我们在 MySaveMenuWidget 里“造反”重写了 GetDesiredInputConfig，
		// 强行返回了 NoCapture，拆除空气墙把鼠标放跑了。
		// 此时 UI 关了，如果不管它，虚幻引擎的鼠标捕获状态机可能依然处于混乱的“放养状态”。
		// 解决方案：必须重新向引擎下达你原本在 TopPlayerController 里写好的初始输入模式，
		// 强迫引擎重新“抓住”鼠标，让单次点击立刻转化为开枪！
		if (CachedPC)
		{
			FInputModeGameAndUI InputModeData;
			// 不锁死鼠标位置，但允许游戏和 UI 共同响应
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// 绝对防御：确保即使恢复到 GameMode，指针该显示时也绝不隐藏
			InputModeData.SetHideCursorDuringCapture(false);
			// 将上方调配好的输入模式数据包 (InputModeData)，正式下发并强塞给玩家控制器的底层状态机。
			CachedPC->SetInputMode(InputModeData);
		}
	}
}

void UMyUIHandlerComponent::HandleSaveMenuCloseRequested()
{
	// 接收 UI 内部的关闭请求（比如点击了取消按钮或成功存档后的自动关闭）
	ToggleSaveMenuWidget(false);
}
#pragma endregion