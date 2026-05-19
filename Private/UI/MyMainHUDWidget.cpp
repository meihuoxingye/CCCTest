// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyMainHUDWidget.h"

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h" // 【新增这一行】：引入垂直框插槽的完整定义！
#include "UI/MyCharacterStatusWidget.h"
#include "Character/TopCharacter.h"

void UMyMainHUDWidget::UpdateSquadList(const TArray<ATopCharacter*>& Members, ATopCharacter* ActiveChar)
{
	// 如果在蓝图里不小心把垂直框删了，或者在编辑器里忘记配置 CharacterWidgetClass 子模板
	// 这行代码会直接安全返回，防止游戏因为空指针访问而直接崩溃
	if (!SquadContainer || !CharacterWidgetClass) return;

	// 当前游戏场景里需要显示几个角色属性条卡片（即当前存活的友军人数）
	int32 Required = Members.Num();
	// 当前 UI 垂直框的里已经存在了几个角色属性条卡片
	// GetChildrenCount 获取垂直框下有几个子控件
	int32 Existing = SquadContainer->GetChildrenCount();

	// 步骤一：按需复用或创建控件
	for (int32 i = 0; i < Required; ++i)
	{
		UMyCharacterStatusWidget* Target = nullptr;

		// 如果当前处理的索引 i 小于现有的控件存量，说明垂直框里本来就有现成的旧卡片。
		// 直接用 GetChildAt(i) 把它捞出来，类型转换（Cast）后直接借给 Target 变量。
		if (i < Existing)
		{
			Target = Cast<UMyCharacterStatusWidget>(SquadContainer->GetChildAt(i));
		}
		// 池中控件不够，再创建新控件加入池
		else
		{
			// 如果当前需求索引 i 超过了现有的卡片数量，说明不够用了
			// 执行 CreateWidget 在内存里分配空间，克隆一个崭新的 CharacterWidgetClass 角色状态实例
			Target = CreateWidget<UMyCharacterStatusWidget>(GetWorld(), CharacterWidgetClass);

			if (Target)
			{
				// 如果创建成功，调用 AddChildToVerticalBox 将新卡片塞进垂直框里排队
				// 同时为其包装一个垂直框组件插槽，以便后续代码设置定制化的垂直框参数
				UVerticalBoxSlot* BoxSlot = SquadContainer->AddChildToVerticalBox(Target);

				if (BoxSlot)
				{
					// 如果拿到了插槽，立刻把插槽的水平对齐方式修改为 HAlign_Left（左对齐）
					// 这就剥夺了垂直框默认把卡片横向拉伸占满全屏的强权，让卡片内部的角色属性 UI 里的尺寸框完美复活并生效
					BoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left);
				}
			}
		}

		// 激活并刷新控件数据
		if (Target)
		{
			// 到这一步，无论 Target 是复用出来的还是新创建的，首先把它设置为 SelfHitTestInvisible（自身可见但不响应鼠标点击） 状态
			Target->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

			// 更新当前绑定角色并触发蓝图层的视觉动画逻辑
			Target->RefreshWidget(Members[i], Members[i] == ActiveChar);
		}
	}

	// 步骤二：对于本帧多出来的空闲控件，不进行销毁，而是进行隐藏折叠以备后用
	// 频繁销毁 UI 组件会造成大量的 CPU 内存碎片，下次队伍人变多时可以直接在步骤一中起死回生
	// 不用担心内存爆炸，玩家控制器里设置了 UPROPERTY()，由于是强引用，母体控制器一死，它的引用链直接归零，GC 就会顺藤摸瓜一次性连带物理销毁
	// 并且由于会复用，UI 组件的数量在达到“历史最高人数”之后，就永远锁死、再也不会增加了
	for (int32 i = Required; i < Existing; ++i)
	{
		if (UWidget* Child = SquadContainer->GetChildAt(i))
		{
			// Collapsed（隐藏且不占用物理排版空间）
			Child->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}