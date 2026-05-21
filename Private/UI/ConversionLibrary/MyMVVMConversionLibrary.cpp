// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ConversionLibrary/MyMVVMConversionLibrary.h"

FSlateBrush UMyMVVMConversionLibrary::Conv_SoftTextureToBrush(TSoftObjectPtr<UTexture2D> InSoftTexture)
{
	FSlateBrush Brush;
	if (!InSoftTexture.IsNull())
	{
		// 5.7+ 异步加载保障与 LWC 优化安全，在主线程执行同步加载作为转换器的标准实现
		Brush.SetResourceObject(InSoftTexture.LoadSynchronous());
	}
	return Brush;
}