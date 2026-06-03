// Fill out your copyright notice in the Description page of Project Settings.


#include "SquadUp/MySquadSubsystem.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "Kismet/GameplayStatics.h"

#include "SquadUp/SquadTypes.h"


// --- 必须加上这一行，它是日志频道的本体 ---
DEFINE_LOG_CATEGORY(LogSquadSystem);


// ==============================================================================
// 核心生命周期 (Core Lifecycle)
// ==============================================================================
#pragma region

void UMySquadSubsystem::Tick(float DeltaTime)
{
    // 针对 60+ 人规模，组队逻辑改为每 0.5 秒执行一次，防止每一帧执行导致掉帧
    GroupingTimer += DeltaTime;

    if (GroupingTimer >= 0.5f)
    {
        UpdateGroupingLogic();
        GroupingTimer = 0.f;
    }
}

#pragma endregion


// ==============================================================================
// 成员注册与管理 (Member Registration & Management)
// ==============================================================================
#pragma region

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
        if (ActiveGroups[i].GetValidMemberCount() < 2) ActiveGroups.RemoveAtSwap(i);
    }
}

#pragma endregion


// ==============================================================================
// 核心组队总线管线 (Core Grouping Pipeline)
// ==============================================================================
#pragma region

void UMySquadSubsystem::UpdateGroupingLogic()
{
    // 第 1 步：打印状态
    PrintSquadStatus();

    // 第 2 步：清理并准备候选人数据
    PrepareCandidates();

    // 若待组队池中没有角色，直接结束
    if (Candidates.Num() == 0) return;

    // 第 3 步：构建当前帧的空间网格数据

    // TMap 使用哈希表的技术，按需分配内存，避免了预先分配一个巨大的二维数组，大大节省了内存开销
    // 但是，数组的内存连续排布，访问效率更高，且成员较少时所占内存比 TMap 更少，所以酌情使用 TMap
    TMap<FIntPoint, TArray<ABaseCharacter*>> GridMap;
    BuildSpatialGrid(GridMap);

    // 第 4 步：执行核心搜寻与匹配建队
    SearchAndFormSquads(GridMap);
}

#pragma endregion


// ==============================================================================
// 空间网格与匹配算法 (Spatial Grid & Matching Algorithm)
// ==============================================================================
#pragma region

void UMySquadSubsystem::PrepareCandidates()
{
    // 当角色弱指针失效后，从待组队池中全部一起清除掉
    Candidates.RemoveAll([](const TWeakObjectPtr<ABaseCharacter>& C) { return !C.IsValid(); });

    // 若待组队池中没有角色，则退出组队逻辑
    if (Candidates.Num() == 0) return;

    for (int32 i = ActiveGroups.Num(); i > 0; i--)
    {
        // 判断数组元素是否有效用 IsValid()
        if (!ActiveGroups[i - 1].Captain.IsValid()) continue;

        if (ActiveGroups[i - 1].GetValidMemberCount() < ActiveGroups[i - 1].FixedMaxCapacity)
        {
            Candidates.Add(ActiveGroups[i - 1].Captain);
        }
        else
        {
            Candidates.RemoveSingleSwap(TWeakObjectPtr<ABaseCharacter>(ActiveGroups[i - 1].Captain));
        }
    }
}

void UMySquadSubsystem::BuildSpatialGrid(TMap<FIntPoint, TArray<ABaseCharacter*>>& OutGridMap) const
{
    // 定义局部 Lambda 函数，输入坐标，得到其在 x、y 方向的第几个栅格上
    // FloorToInt 向下取整，如 1.5 变 1
    // GridSize 是 Lambda 函数外的外部变量，需要 this 指针才能访问此类的成员变量
    auto GetGridIndex = [this](FVector Loc) {
        return FIntPoint(FMath::FloorToInt(Loc.X / GridSize), FMath::FloorToInt(Loc.Y / GridSize));
        };

    // 遍历待组队池中的角色
    for (auto& WeakC : Candidates)
    {
        // 将弱指针转为原始指针，运行效率更高
        // 当取出的是弱指针或智能指针，需要用 Get()
        if (ABaseCharacter* C = WeakC.Get())
        {
            // 根据角色位置计算它所在的栅格索引，如果哈希表中存在这个索引，则返回其对应的 T数组
            // 若不存在，则将其转为 Key，并在开辟临时内存，然后返回这个新创建的 T数组
            // Add 将角色存入 Key 对应的 Value（T数组）中
            OutGridMap.FindOrAdd(GetGridIndex(C->GetActorLocation())).Add(C);
        }
    }
}

void UMySquadSubsystem::SearchAndFormSquads(const TMap<FIntPoint, TArray<ABaseCharacter*>>& OutGridMap)
{
    // 提前预留五个位置，避免 FoundBuffer 在 Add 时频繁扩容导致性能下降
    if (FoundBuffer.Max() < 5)
    {
        // 把 TArray 声明为 UMySquadSubsystem 的类成员变量时，能跨 Tick 内存复用：
        // 利用 Reset() 保留容量： 把数组变成类成员变量后，它就一直存在于内存中。每一帧循环开始前，调用 FoundBuffer.Reset()；
        // Reset() 的底层逻辑是只把数组的“当前元素数量 (Num)”设为 0，但绝不释放底层的“已分配内存容量 (Max)”；
        // 等到下一帧再往数组里 Add 存入数据时，指针会直接覆盖原来的旧内存地址；
        // 因为跳过了耗时的内存申请步骤，所以实现了无性能损耗的跨帧复用。
        FoundBuffer.Reserve(5);
    }

    // 本轮尝试建队（或入队）失败的角色
    TArray<ABaseCharacter*> Lose;

    // 建立队长 - 小队快速索引表，即角色指针 -> 小组结构体指针的映射
    TMap<ABaseCharacter*, FSquadGroup*> CaptainMap;
    for (int32 i = 0; i < ActiveGroups.Num(); ++i)
    {
        if (ActiveGroups[i].Captain.IsValid())
        {
            // 记录队长的指针对应哪个小组，方便后面直接查
            CaptainMap.Add(ActiveGroups[i].Captain.Get(), &ActiveGroups[i]);
        }
    }

    // 处理这种“一边处理一边排空”的池子时，行业标准做法是使用 while
    while (Candidates.Num() > 0)
    {
        // 每次从末尾移除并使其作为一个“领队”，避免数组元素大面积移动
        ABaseCharacter* A = Candidates.Pop().Get();
        if (!A) continue;

        // 【修改此处】：清空成员变量数组的元素数量，但不释放内存，实现跨 Tick 复用内存
        FoundBuffer.Reset();

        // 小组最大组员数
        int32 TargetSize = 0;
        // 在搜索前，先算好这次到底还需要拉几个人
        if (FSquadGroup** ExistingPtr = CaptainMap.Find(A))
        {
            // 如果是老队长，TargetSize = (队伍最大容量 - 队伍目前存活人数) + 1 (因为 Found 里面包含了队长自己)
            TargetSize = (*ExistingPtr)->FixedMaxCapacity - (*ExistingPtr)->GetValidMemberCount() + 1;
        }
        else
        {
            // 纯新人，随机摇一个目标人数
            TargetSize = A->GetAttributeConfig()->GetRandomSquadSize();
        }

        FoundBuffer.Add(A);

        // 获取 A 的位置
        FVector ALoc = A->GetActorLocation();
        // 获取 A 的组队半径的平方
        float RadiusSq = FMath::Square(A->GetAttributeConfig()->GroupingRadius);

        // --- 核心优化：象限判断逻辑 (2x2 搜索) ---
        // 通过坐标与栅格起点的偏移，判断邻居可能存在的方向
        int32 GridX = FMath::FloorToInt(ALoc.X / GridSize);
        int32 GridY = FMath::FloorToInt(ALoc.Y / GridSize);

        // 算出在该栅格内的相对位置（0.0 ~ 1.0）
        // 如果偏移小于 0.5，说明靠左/靠下，应检查 -1 方向；反之检查 +1
        int32 OffsetX = ((ALoc.X / GridSize) - GridX < 0.5f) ? -1 : 1;
        int32 OffsetY = ((ALoc.Y / GridSize) - GridY < 0.5f) ? -1 : 1;

        // 只定义 4 个需要搜索的栅格索引结构体
        FIntPoint CellsToCheck[4] = {
            {GridX, GridY},           // 当前格
            {GridX + OffsetX, GridY}, // 水平邻格
            {GridX, GridY + OffsetY}, // 垂直邻格
            {GridX + OffsetX, GridY + OffsetY} // 对角邻格
        };

        // 扁平化搜索：只看这 4 个格子
        for (const FIntPoint& TargetIdx : CellsToCheck)
        {
            // Find 查找哈希表里 Key栅格索引 对应的 Value角色
            if (const auto* Cell = OutGridMap.Find(TargetIdx))
            {
                for (ABaseCharacter* B : *Cell)
                {
                    if (B == A || FoundBuffer.Contains(B)) continue;

                    // 计算 A 和 B 的距离平方，避免开根号的性能消耗
                    if (FVector::DistSquared(ALoc, B->GetActorLocation()) < RadiusSq)
                    {
                        FoundBuffer.Add(B);

                        // 优化点：队员一旦入组，立刻从待组队池中抽离
                        // RemoveSingleSwap 会把末尾元素挪过来填补空缺，时间复杂度 O(N) 但比重排数组快得多
                        Candidates.RemoveSingleSwap(TWeakObjectPtr<ABaseCharacter>(B));

                        // 本次找到的人数已达到目标指标，跳出内层循环
                        if (FoundBuffer.Num() >= TargetSize) break;
                    }
                }
            }

            // 招募名额已满,足以补齐小队，无需继续搜索剩下的栅格
            if (FoundBuffer.Num() >= TargetSize) break;
        }

        if (FoundBuffer.Num() >= 2)
        {
            // 如果 A 已经是老队长，直接把新成员加到原来的小组里
            if (FSquadGroup** FoundGroupPtr = CaptainMap.Find(A))
            {
                // 双重指针装换为原始指针，拿到原来小组的指针
                FSquadGroup* ExistingGroup = *FoundGroupPtr;
                for (ABaseCharacter* Newbie : FoundBuffer)
                {
                    if (Newbie == A) continue;

                    // 若小组未满员，则从 found池 加入新成员
                    ExistingGroup->TryAddMember(Newbie);
                }
            }
            else
            {
                // 纯新人建新组
                FSquadGroup NewGroup;
                for (auto* M : FoundBuffer) NewGroup.Members.Add(M);
                NewGroup.Captain = A;

                NewGroup.FixedMaxCapacity = TargetSize;
                NewGroup.AnchorLocation = ALoc;
                ActiveGroups.Add(NewGroup);

                // 注意：由于 Add 可能会导致 ActiveGroups 内存重排，
                // 如果后面还要用到 CaptainMap，建议重新构建或直接结束本轮。
                // 因为是 Candidates.Pop() 模式，这里直接 break 出去处理下一个人是安全的。
            }
        }
        else
        {
            // 没凑够 2 个人，A 继续回池子待命
            Lose.AddUnique(A);
        }
    }

    // 将没成队的角色重新加入待组队池
    while (Lose.Num() > 0)
    {
        ABaseCharacter* C = Lose.Pop();

        // Lose 是从 Candidates 里 Pop 出来的，确定不在池子里，直接 Add 即可，省去 AddUnique 查重开销
        if (C) Candidates.Add(C);
    }
}

#pragma endregion


// ==============================================================================
// 日志与调试 (Logging & Debugging)
// ==============================================================================
#pragma region

void UMySquadSubsystem::PrintSquadStatus() const
{
    // 实时打印小队状态到 Output Log
    // 打印一个分割线，方便在日志里区分不同时间点的数据
    UE_LOG(LogSquadSystem, Log, TEXT("--------- 实时战术面板 (%d 个小组) ---------"), ActiveGroups.Num());
    UE_LOG(LogSquadSystem, Log, TEXT("--------- 待组队池 (%d 个人) ---------"), Candidates.Num());

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