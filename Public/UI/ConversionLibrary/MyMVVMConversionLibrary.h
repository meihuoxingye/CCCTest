// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "MyMVVMConversionLibrary.generated.h"


/**
 * 
 */
UCLASS()
class CCC_API UMyMVVMConversionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// 合法 MVVM 转换器函数，把 ViewModel 里的 Texture2D 图片硬盘路径 (TSoftObjectPtr)，转换成 UI Image 控件唯一能读取渲染的“UI 笔刷 (FSlateBrush)
	// 本质为惰性加载，只有当 UI 刷新时才会播放广播，然后真正加载图片资源并转换成笔刷
	// BlueprintPure 纯函数相当于一个纯数学公式，保证了 UI 刷新时绝对不会篡改游戏世界的后台数据（无副作用）
	// 在 UI 蓝图资产的视图绑定界面设置使用。必须标记 meta = (MVVMAllowed)，这是被 UI 绑定面板识别的通行证
	UFUNCTION(BlueprintPure, Category = "MVVM|Conversion", meta = (MVVMAllowed))
	static struct FSlateBrush Conv_SoftTextureToBrush(TSoftObjectPtr<UTexture2D> InSoftTexture);
};
