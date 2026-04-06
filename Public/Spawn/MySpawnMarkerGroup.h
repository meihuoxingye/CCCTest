// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySpawnMarkerGroup.generated.h"

UCLASS()
class CCC_API AMySpawnMarkerGroup : public AActor
{
	GENERATED_BODY()
	
public:
    AMySpawnMarkerGroup();

protected:
    // 游戏开始时执行
    virtual void BeginPlay() override;
    // 游戏结束或此管理器被销毁时执行
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 定义一个空的场景组件作为根节点，方便在编辑器里移动整个管理器
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class USceneComponent> RootComp;

    // 添加一个默认的编辑器图标（可选，让你在场景里更容易找到它）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UBillboardComponent> ManagerIcon;

};
