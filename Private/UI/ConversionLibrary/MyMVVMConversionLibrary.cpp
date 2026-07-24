// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ConversionLibrary/MyMVVMConversionLibrary.h"

FSlateBrush UMyMVVMConversionLibrary::Conv_SoftTextureToBrush(TSoftObjectPtr<UTexture2D> InSoftTexture)
{
	// 准备一个空的 UI 笔刷外壳
	FSlateBrush Brush;

	// 检查数据资产里是否配置了头像路径，防止加载空指针
	if (!InSoftTexture.IsNull())
	{
		// 顺着路径去硬盘同步加载真实图片，并直接塞进 UI 笔刷里打包
		// 执行逻辑为惰性加载，只有当 UI 刷新时才会播放广播，然后真正加载图片资源并转换成笔刷
		// 具体实现为同步加载 LoadSynchronous，在主线程执行同步加载作为转换器是标准实现
		// 同步加载会导致卡顿，不加载完就会一直卡在这里；异步加载不会卡顿，但会渲染上默认的黑色图片，黑色图片问题可以用动画特效或延迟画面等方法解决
		Brush.SetResourceObject(InSoftTexture.LoadSynchronous());
	}
	return Brush;
}