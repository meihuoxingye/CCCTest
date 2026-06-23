// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// 必须在 .h 里包含，不能只在 .cpp 里包含
#include "UI/MyCharacterViewModel.h"

#include "MyCharacterStatusWidget.generated.h"

/** * 双轨制 UI 中继站。负责把低频数据塞进 MVVM 管线，把高频快照数据直接拍给 GPU 材质，彻底消灭 UI Tick
 */
UCLASS()
class CCC_API UMyCharacterStatusWidget : public UUserWidget
{
	GENERATED_BODY()


	// ==============================================================================
	// 核心生命周期与初始化 (Core Lifecycle & Initialization)
	// ==============================================================================
public:
	// 外部总控（MyMainHUDWidget）在创建此卡片时调用的初始化入口
	// 负责同步角色的基础静态数据，并拉起两套管线（MVVM + 材质事件）
	UFUNCTION(BlueprintCallable, Category = "CharacterUI")
	void SyncViewModel(class ATopCharacter* InCharacter, bool bSelected);

protected:
	// 负责完成底层蓝图动态绑定，并向全局单例 (Subsystem) 总线插上初始监听网线；
	// NativeOnInitialized 在底层 Slate 控件被创建时触发，终其一生仅执行一次
	virtual void NativeOnInitialized() override;

	// UI 死亡前的遗言函数：专门用来安全解绑全局委托，防止 UI 销毁后子系统还在给它发广播导致野指针崩溃
	virtual void NativeDestruct() override;

private:
	// 缓存绑定的角色 ID：这是过滤“广播噪音”的身份证。场上几十个人发广播，只听自己发的
	UPROPERTY()
	FName CachedCharacterID;

	// 缓存当前 UI 卡片对应的具体角色实体；
	// 弱指针不干扰生死（防泄漏），不增加对象的引用计数，这个指针无论使用状态如何，都不影响 GC 销毁本体角色，所以绝对不会产生僵尸内存；
	// 弱指针自带生命感知（防崩溃），当角色实体被摧毁时立刻访问它，返回的状态标记也会为“失效”，便于条件检查；
	// 不经过垃圾回收器（GC），所以必须删除 UPROPERTY() 宏；
	// 缺点：访问前必须先检查有效性，否则可能访问到无效指针；蓝图不可用；性能损耗
	TWeakObjectPtr<class ATopCharacter> CachedCharacterRef;


	// ==============================================================================
	// MVVM 视图模型数据层 (MVVM Data Layer)
	// ==============================================================================
private:
	// MVVM 数据盒子：强引用视图模型，处理血量、头像等发生频率极低（触发式）的离散数据
	UPROPERTY()
	TObjectPtr<class UMyCharacterViewModel> CharacterVM;


	// ==============================================================================
	// 高频 SP 材质渲染管线 (High-Frequency SP Material Pipeline)
	// ==============================================================================
protected:
	// 监听器函数：听到子系统的 SP 变化广播后，触发此函数 (事件驱动)
	void OnSPDataChanged(FName CharacterID, float NewSPPercent);

	// 推送相关参数给材质，通过材质节点利用 GPU 模拟 SP 进度条变化；
	// 核心搬运工：从子系统把最新快照拉出来，绕过 CPU 计算，直接喂给 GPU 材质实例；
	// 处理屏幕上几万个像素的简单计算，天生擅长大规模并行计算的 GPU 最适合，所以用材质来当 UI
	void RefreshSPDataFromSubsystem();

private:
	// 核心绑定宏 meta = (BindWidget)：
	// 强制要求 UI 蓝图中必须有一个名字一模一样叫 "SPProgressBarImage" 的 Image 控件，否则直接编译报错（蓝图与 C++ 的强捆绑）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UImage> SPProgressBarImage;

	// 材质动态实例 (MID)：连接 C++ 与显存渲染的专属桥梁，用来向 GPU 传递底层快照参数
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> SP_MID;

	// 精准卸载句柄；
	// 绑定委托时生成对应专属 ID 并赋给它，UI 销毁时拿着它就能在子系统里精准解除监听
	FDelegateHandle SPChangedHandle;


	// ==============================================================================
	// 角色切换总线与用户交互 (Character Switch Bus & User Interaction)
	// ==============================================================================

private:
	// 强绑定蓝图中的头像隐形按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> AvatarButton;

	// 当玩家鼠标点击了这张头像卡片时，触发此函数；
	// 必须加 UFUNCTION()！因为供蓝图 Button 的 OnClicked 动态多播委托绑定，需经过引擎反射系统；
	// 【架构演进】：彻底摒弃旧版的全局总线广播！现改由 IMyPlayerUIInterface 契约接口;
	// 点对点向专属的 ATopPlayerController::RequestCharacterSwitch_Implementation() 转接到具体处理角色附身逻辑;
	// 纪律重申：UI 至此绝不立刻修改高亮表现！盲发请求后，必须按兵不动，静候引擎底层仲裁后的“下行事实”广播。
	UFUNCTION()
	void OnAvatarClicked();


	// 接收到主控角色变更后的处理函数；
	// 无需加 UFUNCTION()，因在 cpp 中未直接绑定此函数指针，而是通过 Lambda 进行调用。
	// 原生 C++ 委托，完全绕过了沉重的反射系统，执行效率极高
	void HandleActiveCharacterChanged(ATopCharacter* NewActiveChar);

	// 【下行事实频道退订凭证句柄】
	// 本质：UI 成功订阅 OnActiveCharacterChanged (下行事实频道) 时，由全局总线颁发的唯一单号。
	// 唯一使命：在 UI 生命周期结束 (NativeDestruct) 时，执此句柄调用 Remove 解绑，防野指针崩溃。
	FDelegateHandle ActiveCharChangedHandle;
};