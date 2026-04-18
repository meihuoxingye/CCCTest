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
    TArray<ABaseCharacter*> Found; // --- 优化点：Found 池移出循环，复用内存 ---
    Found.Reserve(4);

    // --- 优化点：改用 while 循环，实现即时从待组队池移除 ---
    while (Candidates.Num() > 0)
    {
        // 每次从末尾弹出一个“领队”，避免数组元素大面积移动
        ABaseCharacter* A = Candidates.Pop().Get();

        // 如果该角色在之前的循环中已作为“队员”被拉进别的小组，则跳过
        if (!A || Processed.Contains(A)) continue;

        Found.Reset();
        Found.Add(A);

        FVector ALoc = A->GetActorLocation();
        float RadiusSq = FMath::Square(A->GetAttributeConfig()->GroupingRadius); //
        ECharacterType AType = A->GetAttributeConfig()->CharacterType;

        // --- 核心优化：象限判断逻辑 (2x2 搜索) ---
        // 通过坐标与栅格起点的偏移，判断邻居可能存在的方向
        int32 GridX = FMath::FloorToInt(ALoc.X / GridSize);
        int32 GridY = FMath::FloorToInt(ALoc.Y / GridSize);

        // 算出在该栅格内的相对位置（0.0 ~ 1.0）
        // 如果偏移小于 0.5，说明靠左/靠下，应检查 -1 方向；反之检查 +1
        int32 OffsetX = ((ALoc.X / GridSize) - GridX < 0.5f) ? -1 : 1;
        int32 OffsetY = ((ALoc.Y / GridSize) - GridY < 0.5f) ? -1 : 1;

        // 只定义 4 个需要搜索的栅格索引
        FIntPoint CellsToCheck[4] = {
            {GridX, GridY},           // 当前格
            {GridX + OffsetX, GridY}, // 水平邻格
            {GridX, GridY + OffsetY}, // 垂直邻格
            {GridX + OffsetX, GridY + OffsetY} // 对角邻格
        };

        // 扁平化搜索：只看这 4 个格子
        for (const FIntPoint& TargetIdx : CellsToCheck)
        {
            if (TArray<ABaseCharacter*>* Cell = GridMap.Find(TargetIdx))
            {
                for (ABaseCharacter* B : *Cell)
                {
                    if (B == A || Processed.Contains(B)) continue;
                    if (B->GetAttributeConfig()->CharacterType != AType) continue;

                    if (FVector::DistSquared(ALoc, B->GetActorLocation()) < RadiusSq)
                    {
                        Found.Add(B);
                        Processed.Add(B);
                        // --- 优化点：队员一旦入组，立刻从待组队池中抽离 ---
                        // RemoveSingleSwap 会把末尾元素挪过来填补空缺，时间复杂度 O(N) 但比重排数组快得多
                        Candidates.RemoveSingleSwap(TWeakObjectPtr<ABaseCharacter>(B));

                        if (Found.Num() >= 4) break;
                    }
                }
            }
            if (Found.Num() >= 4) break;
        }

        if (Found.Num() >= 2)
        {
            FSquadGroup NewGroup;
            Processed.Add(A); // 领队也要标记
            for (auto* M : Found) NewGroup.Members.Add(M);

            // 解决你之前反馈的“回头望”：直接以领队 A 为锚点
            NewGroup.AnchorLocation = ALoc;
            ActiveGroups.Add(NewGroup);
        }
    }
}

void UMySquadSubsystem::UpdateMovementLogic(float DeltaTime)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;
    FVector PlayerLoc = Player->GetActorLocation();

    // 1. 计算小组间的排斥偏置
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
                // ✅ 解决“推力太强”：大幅降低系数 (0.5f -> 0.1f)，让避让变平滑
                FVector Push = (Dir / (Dist + 0.1f)) * (MinDist - Dist) * 0.1f;
                GroupSeparations[i] += Push;
                GroupSeparations[j] -= Push;
            }
        }
    }

    for (int32 i = 0; i < ActiveGroups.Num(); ++i)
    {
        auto& Group = ActiveGroups[i];
        if (Group.Members.Num() == 0) continue;

        ABaseCharacter* Representative = nullptr;
        for (auto& M : Group.Members) { if (M.IsValid()) { Representative = M.Get(); break; } }
        if (!Representative) continue;

        bool bCanMove = (Representative->GetAttributeConfig()->AIDetectionLevel == EAIDetectionLevel::NoPerception || Group.bIsAggro);
        if (!bCanMove) continue;

        FVector CurrentCenter = GetGroupCenter(Group);
        FVector DirFromPlayer = (CurrentCenter - PlayerLoc).GetSafeNormal();

        // ✅ 解决“紧贴角色”：把停止距离从 100 调大到 600~800
        float StopDistance = 600.f;

        float OffsetAngle = (i % 2 == 0 ? 1.0f : -1.0f) * (i * 0.5f);
        FVector SurroundPos = DirFromPlayer.RotateAngleAxis(OffsetAngle * 25.f, FVector::UpVector) * StopDistance;

        // ✅ 解决“挤在一起”：给排斥力加限制，防止目的地被推飞
        FVector FinalTarget = PlayerLoc + SurroundPos + GroupSeparations[i].GetClampedToMaxSize(500.f);

        // ✅ 解决“不跟着走”的关键：插值起点必须是 AnchorLocation 自身！
        // 不能用 CurrentCenter，否则锚点永远会被 AI 的延迟位置拉回去
        FVector Diff = Group.AnchorLocation - FinalTarget;
        bool bInRect = FMath::Abs(Diff.X) < 600.f && FMath::Abs(Diff.Y) < 400.f;

        if (!bInRect)
        {
            // 锚点自己向前插值，引导 AI 追赶
            Group.AnchorLocation = FMath::VInterpTo(Group.AnchorLocation, FinalTarget, DeltaTime, 1.5f);
        }
        else
        {
            Group.AnchorLocation = FMath::VInterpTo(Group.AnchorLocation, FinalTarget, DeltaTime, 0.5f);

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