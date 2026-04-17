// Fill out your copyright notice in the Description page of Project Settings.


#include "SquadUp/MySquadSubsystem.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "Kismet/GameplayStatics.h"


// --- 必须加上这一行，它是日志频道的本体 ---
DEFINE_LOG_CATEGORY(LogSquadSystem);

void UMySquadSubsystem::RegisterCandidate(ABaseCharacter* Character) 
{ 
    // AddUnique 在加入之前会先“扫视”一遍数组，如果不存在，把它加进去；如果已经存在，什么都不做
    if (Character) Candidates.AddUnique(Character); 
}

void UMySquadSubsystem::UnregisterCharacter(ABaseCharacter* Character)
{
    Candidates.Remove(Character);
    RemoveFromGroup(Character);
}

void UMySquadSubsystem::RemoveFromGroup(ABaseCharacter* Character)
{
    // 从后往前遍历删除
    for (int32 i = ActiveGroups.Num() - 1; i >= 0; --i)
    {
        // 从小组池中的小组的成员数组中移除角色
        ActiveGroups[i].Members.Remove(Character);

        // 若移除后只有一人或以下则顺便解散小组
        // RemoveAtSwap 性能更好，删除后直接把末位的元素放到被删除的位置上，不需要移动其他元素
        // 但这样会改变元素的顺序，所以只能在不关心顺序的情况下使用
        if (ActiveGroups[i].Members.Num() < 2) ActiveGroups.RemoveAtSwap(i);
    }
}

void UMySquadSubsystem::Tick(float DeltaTime)
{
    // 针对 60+ 人规模，组队逻辑改为每 0.5 秒执行一次，防止每一帧执行导致掉帧
    static float GroupingTimer = 0.f;
    GroupingTimer += DeltaTime;
    if (GroupingTimer >= 0.5f)
    {
        UpdateGroupingLogic();
        GroupingTimer = 0.f;

        #pragma region 日志输出小组成员
        // 实时打印小队状态到 Output Log
        // 我们可以复用 0.2s 的计时器，没必要每帧都刷日志，否则滚动太快看不清
        static float LogTimer = 0.f;
        LogTimer += DeltaTime;
        if (LogTimer >= 0.2f)
        {
            LogTimer = 0.f;

            // 打印一个分割线，方便在日志里区分不同时间点的数据
            UE_LOG(LogSquadSystem, Log, TEXT("--------- 实时战术面板 (%d 个小组) ---------"), ActiveGroups.Num());

            for (int32 i = 0; i < ActiveGroups.Num(); ++i)
            {
                FString MemberList;
                for (auto& M : ActiveGroups[i].Members)
                {
                    if (M.IsValid()) MemberList += FString::Printf(TEXT("[%s] "), *M->GetName());
                }

                // 输出到日志：小组索引、成员数、具体的成员名字
                UE_LOG(LogSquadSystem, Log, TEXT("小组 %d | 成员数: %d | 成员列表: %s"), i, ActiveGroups[i].Members.Num(), *MemberList);
            }
        }
        #pragma endregion
    }

    UpdateMovementLogic(DeltaTime);
}

void UMySquadSubsystem::UpdateGroupingLogic()
{
    // 当角色弱指针失效后，从待组队池中全部一起清除掉
    Candidates.RemoveAll([](const TWeakObjectPtr<ABaseCharacter>& C) { return !C.IsValid(); });

    // 若待组队池中没有角色，则退出组队逻辑
    if (Candidates.Num() == 0) return;

    // 定义局部 Lambda 函数，输入坐标，得到其在 x、y 方向的第几个栅格上
    // FloorToInt 向下取整，如 1.5 变 1
    // GridSize 是 Lambda 函数外的外部变量，需要 this 指针才能访问此类的成员变量
    auto GetGridIndex = [this](FVector Loc) {
        return FIntPoint(FMath::FloorToInt(Loc.X / GridSize), FMath::FloorToInt(Loc.Y / GridSize));
        };

    // TMap 使用哈希表的技术，按需分配内存，避免了预先分配一个巨大的二维数组，大大节省了内存开销
    // 但是，数组的内存连续排布，访问效率更高，且成员较少时所占内存比 TMap 更少，所以酌情使用 TMap
    TMap<FIntPoint, TArray<ABaseCharacter*>> GridMap;
    // 遍历待组队池中的角色
    for (auto& WeakC : Candidates)
    {
        // 将弱指针转为原始指针，运行效率更高
        if (ABaseCharacter* C = WeakC.Get())
        {
            // 根据角色位置计算它所在的栅格索引，将其转为 Key，并开辟临时内存
            // Add 将角色存入 Key 对应的 Value 中
            GridMap.FindOrAdd(GetGridIndex(C->GetActorLocation())).Add(C);
        }
    }

    TSet<ABaseCharacter*> Processed;

    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        ABaseCharacter* A = Candidates[i].Get();
        if (!A || Processed.Contains(A)) continue;

        TArray<ABaseCharacter*> Found;
        Found.Add(A);

        FIntPoint CenterIdx = GetGridIndex(A->GetActorLocation());
        float RadiusSq = FMath::Square(A->GetAttributeConfig()->GroupingRadius);

        // 只搜索自身及相邻的 3x3 栅格
        for (int32 x = -1; x <= 1; ++x)
        {
            for (int32 y = -1; y <= 1; ++y)
            {
                if (TArray<ABaseCharacter*>* Cell = GridMap.Find(CenterIdx + FIntPoint(x, y)))
                {
                    for (ABaseCharacter* B : *Cell)
                    {
                        if (B == A || Processed.Contains(B)) continue;

                        if (A->GetAttributeConfig()->CharacterType == B->GetAttributeConfig()->CharacterType)
                        {
                            // 使用距离平方比较，省去 60+ 人规模下昂贵的开根号运算
                            if (FVector::DistSquared(A->GetActorLocation(), B->GetActorLocation()) < RadiusSq)
                            {
                                Found.Add(B);
                                if (Found.Num() >= 4) break;
                            }
                        }
                    }
                }
                if (Found.Num() >= 4) break;
            }
            if (Found.Num() >= 4) break;
        }

        if (Found.Num() >= 2)
        {
            FSquadGroup NewGroup;
            FVector Sum = FVector::ZeroVector;
            for (auto* M : Found)
            {
                NewGroup.Members.Add(M);
                Sum += M->GetActorLocation();
                Processed.Add(M);
            }
            NewGroup.AnchorLocation = Sum / (float)Found.Num();
            ActiveGroups.Add(NewGroup);
        }
    }

    // 批量移除已入组的成员，比在循环内逐个 Remove 性能高得多
    Candidates.RemoveAll([&Processed](const TWeakObjectPtr<ABaseCharacter>& C) {
        return !C.IsValid() || Processed.Contains(C.Get());
        });
}

void UMySquadSubsystem::UpdateMovementLogic(float DeltaTime)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;
    FVector PlayerLoc = Player->GetActorLocation();

    // --- 新增逻辑：计算小组间的排斥偏置，确保队与队之间隔开 ---
    TArray<FVector> GroupSeparations;
    GroupSeparations.SetNumZeroed(ActiveGroups.Num());

    for (int32 i = 0; i < ActiveGroups.Num(); ++i)
    {
        for (int32 j = i + 1; j < ActiveGroups.Num(); ++j)
        {
            FVector Dir = ActiveGroups[i].AnchorLocation - ActiveGroups[j].AnchorLocation;
            float DistSq = Dir.SizeSquared2D();
            float MinDist = 1200.f;
            if (DistSq < FMath::Square(MinDist))
            {
                float Dist = FMath::Sqrt(DistSq);
                FVector Push = (Dir / (Dist + 0.1f)) * (MinDist - Dist) * 0.5f;
                GroupSeparations[i] += Push;
                GroupSeparations[j] -= Push;
            }
        }
    }

    // --- 下面进入你原本的逻辑循环 ---
    for (int32 i = 0; i < ActiveGroups.Num(); ++i)
    {
        auto& Group = ActiveGroups[i];

        // 1. 基础安全检查：如果小组里没人，直接跳过
        if (Group.Members.Num() == 0) continue;

        // 2. 检测与仇恨逻辑
        ABaseCharacter* Representative = nullptr;
        for (auto& M : Group.Members) { if (M.IsValid()) { Representative = M.Get(); break; } }
        if (!Representative) continue;

        bool bCanMove = (Representative->GetAttributeConfig()->AIDetectionLevel == EAIDetectionLevel::NoPerception || Group.bIsAggro);
        if (!bCanMove) continue;

        // --- 核心修改：动态更新锚点位置 ---

        // 3. 计算当前的物理重心
        FVector CurrentCenter = GetGroupCenter(Group);

        // ---【新增：解决紧贴与挤在一起的关键】---
        // 计算从玩家指向当前小队的向量
        FVector DirFromPlayer = (CurrentCenter - PlayerLoc).GetSafeNormal();

        // 设定停止距离
        float StopDistance = 100.f;

        // 计算“环绕偏置”：让不同的小队在圆周上错开，防止挤在一条线上
        // 利用小组索引 i 来计算一个左右偏转角（比如每组偏 30 度）
        float OffsetAngle = (i % 2 == 0 ? 1.0f : -1.0f) * (i * 0.5f);
        FVector SurroundPos = DirFromPlayer.RotateAngleAxis(OffsetAngle * 20.f, FVector::UpVector) * StopDistance;

        // 4. 计算锚点的目标偏移
        // FinalTarget 现在是：玩家位置 + 8米开外的环绕点 + 队间排斥力
        FVector FinalTarget = PlayerLoc + SurroundPos + GroupSeparations[i];

        FVector Diff = CurrentCenter - FinalTarget;
        bool bInRect = FMath::Abs(Diff.X) < 600.f && FMath::Abs(Diff.Y) < 400.f;

        if (!bInRect)
        {
            // 在远处时，锚点以较快速度向玩家逼近
            Group.AnchorLocation = FMath::VInterpTo(CurrentCenter, FinalTarget, DeltaTime, 1.5f);
        }
        else
        {
            // 进入战术范围内，锚点移动变慢，并开始加入你之前的随机晃动（Jitter）
            Group.AnchorLocation = FMath::VInterpTo(CurrentCenter, FinalTarget, DeltaTime, 0.5f);

            Group.YTimer += DeltaTime;
            FVector Wobble(
                FMath::Cos(Group.YTimer * 0.7f) * 120.f * DeltaTime,
                FMath::Sin(Group.YTimer * 1.3f) * 150.f * DeltaTime,
                0.f
            );
            Group.AnchorLocation += Wobble;
        }
    }
}

void UMySquadSubsystem::SetGroupAggro(ABaseCharacter* Member, bool bNewAggro)
{
    for (auto& G : ActiveGroups) { if (G.Members.Contains(Member)) { G.bIsAggro = bNewAggro; break; } }
}

FVector UMySquadSubsystem::GetTacticalLocation(ABaseCharacter* Character)
{
    for (auto& Group : ActiveGroups)
    {
        int32 Idx = Group.Members.Find(Character);
        if (Idx != INDEX_NONE) return Group.AnchorLocation + FVector(Idx * -160.f, Idx * 120.f, 0.f);
    }
    return Character->GetActorLocation();
}

FVector UMySquadSubsystem::GetGroupCenter(const FSquadGroup& Group) const
{
    FVector Sum = FVector::ZeroVector;
    int32 Count = 0;
    for (const auto& M : Group.Members) { if (M.IsValid()) { Sum += M->GetActorLocation(); Count++; } }

    // 防止除以 0（万一小组里没人了）
    return (Count > 0) ? (Sum / (float)Count) : FVector::ZeroVector;
}