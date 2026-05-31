// Fill out your copyright notice in the Description page of Project Settings.


#include "SkillSystem/SkillPointSubsystem.h"

// 识别 UWorld 指针
#include "Engine/World.h"

void USkillPointSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // 调用父类初始化
    Super::Initialize(Collection);

    // 定义测试角色结构体并加入哈希表
    FCharacterSPData WarriorData;
    WarriorData.LastSyncGameTime = 0.0;
    SquadSPMap.Add(TEXT("Hero_Warrior"), WarriorData);
}


float USkillPointSubsystem::GetCalculatedCharacterSP(FName CharacterID) const
{
    // 【性能优化】：使用 Find 返回指针。只算 1 次哈希值，完爆 Contains + [] 的 2 次开销
    const FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr) return 0.0f;

    // 如果处于冻结状态（安全区/播动画），或者世界还没加载好，直接返回之前冻结的死数据
    if (DataPtr->bIsRegenFrozen || !GetWorld())
    {
        return DataPtr->SavedSP;
    }

    // 获取现在的游戏时间戳（受全局子弹时间影响）
    double CurrentGameTime = GetWorld()->GetTimeSeconds();
    // 算时间差 = 现在 - 上次同步的时间
    double TimePassed = CurrentGameTime - DataPtr->LastSyncGameTime;

    // 【安全兜底】：防止存档读档错乱导致时间差变成负数，从而倒扣技能点
    if (TimePassed < 0.0) TimePassed = 0.0;

    // 惰性求值，不依赖 Tick，而是根据时间差动态推算
    // 公式：当前实时 SP = 快照老底 + (过去的时间差 * 恢复速度)
    float CalculatedSP = DataPtr->SavedSP + (static_cast<float>(TimePassed) * DataPtr->RegenRate);
    
    // 用 Clamp 把最终结果卡死在 0 到 MaxSP 之间，防止涨爆了
    return FMath::Clamp(CalculatedSP, 0.0f, DataPtr->MaxSP);
}

float USkillPointSubsystem::GetCharacterSPPercent(FName CharacterID) const
{
    // 单次哈希查找
    const FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    // 防除以 0 崩溃：如果人不存在，或者最大上限是 0，直接返回 0%
    if (!DataPtr || DataPtr->MaxSP <= 0.0f) return 0.0f;

    // 获取实时 SP，除以最大值得到百分比
    float CurrentSP = GetCalculatedCharacterSP(CharacterID);
    return CurrentSP / DataPtr->MaxSP;
}


// 向下取整当前 SP，给 UI 文本显示用
int32 USkillPointSubsystem::GetCurrentSPAsInt(FName CharacterID) const
{
    return FMath::FloorToInt(GetCalculatedCharacterSP(CharacterID));
}


bool USkillPointSubsystem::ConsumeCharacterSP(FName CharacterID, float Amount)
{
    // 在哈希表查找角色数据，拿到对应技能点结构体指针
    FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr) return false;

    // GetCharacterSP 是 const 的，在这里安全调用获取实时 SP
    float CurrentSP = GetCalculatedCharacterSP(CharacterID);

    // 如果当前 SP 足够扣除，直接从快照底座扣掉，并且对齐时间轴到现在
    if (CurrentSP >= Amount)
    {
        // 记录本次技能点花费后的新快照底数
        DataPtr->SavedSP = CurrentSP - Amount;
        // 记录本次操作的时间戳，作为新的同步基准点
        DataPtr->LastSyncGameTime = GetWorld()->GetTimeSeconds();

        // 【新增广播】：通知 UI，技能点被扣了，必须立刻呼叫 UI 蓝图重新拉取最新的快照进行重绘
        // ==========================================
        OnSPChanged.Broadcast(CharacterID, GetCharacterSPPercent(CharacterID));

        return true;
    }

    // 如果 SP 不够，扣除失败，返回 false
    return false;
}

void USkillPointSubsystem::SetCharacterRegenFrozen(FName CharacterID, bool bFreeze)
{
    // 查找角色数据指针
    FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr || DataPtr->bIsRegenFrozen == bFreeze) return;

    // 如果要冻结
    if (bFreeze)
    {
        // 瞬间把现在算出来的实时值，定死保存在 SavedSP 快照点数里
        DataPtr->SavedSP = GetCalculatedCharacterSP(CharacterID);
        // 设置状态为冻结
        DataPtr->bIsRegenFrozen = true;
    }
    // 如果要解冻
    else
    {
        // 把时间戳对齐到出安全区的这一秒
        DataPtr->LastSyncGameTime = GetWorld()->GetTimeSeconds();
        // 设置状态为解冻
        DataPtr->bIsRegenFrozen = false;
    }
}

void USkillPointSubsystem::SetAllCharactersRegenFrozen(bool bFreeze)
{
    TArray<FName> Keys;
    // 让哈希表把里面目前存着的所有 Key 一口气全部复制到上面的数组里
    SquadSPMap.GetKeys(Keys);
    // 遍历这个 Key 数组，批量设置每个角色的技能点恢复状态
    for (const FName& Key : Keys)
    {
        SetCharacterRegenFrozen(Key, bFreeze);
    }
}

// 为存档做准备，强制把所有角色的技能点数据同步到当前时间点，并冻结技能点恢复功能
void USkillPointSubsystem::PrepareForSave()
{
    TArray<FName> Keys;
    SquadSPMap.GetKeys(Keys);
    for (const FName& Key : Keys)
    {
        // 设置为冻结并且把当前实时 SP 固化到快照里，时间轴对齐到现在
        SetCharacterRegenFrozen(Key, true);
    }
}

// 读档后对齐时间轴，解冻技能点恢复功能
void USkillPointSubsystem::PostLoadSync()
{
    TArray<FName> Keys;
    SquadSPMap.GetKeys(Keys);
    for (const FName& Key : Keys)
    {
        FCharacterSPData* DataPtr = SquadSPMap.Find(Key);
        if (DataPtr)
        {
            // 【灵魂的一行】：强行把存档里带过来的老时间戳，抹杀并覆盖为当前新关卡的最新初始时间
            // 从冻结到解冻，无论是 SetCharacterRegenFrozen 还是读档，都会调用这一行来对齐时间轴
            DataPtr->LastSyncGameTime = GetWorld()->GetTimeSeconds();
            DataPtr->bIsRegenFrozen = false;
        }
    }
}

void USkillPointSubsystem::AddNewCharacterToSquad(FName CharacterID, float MaxSP, float InitialSP, float RegenRate)
{
    // FindOrAdd: 如果哈希表里没这个人（新加入），就当场开辟空间生成；如果已经有了，直接返回它
    FCharacterSPData& NewData = SquadSPMap.FindOrAdd(CharacterID);

    // 只有在时间轴从未同步过的“全新角色”状态下，才赋初始值
    if (NewData.LastSyncGameTime == 0.0)
    {
        NewData.MaxSP = MaxSP;
        // 设置快照点数，作为初始技能点数
        NewData.SavedSP = InitialSP;
        NewData.RegenRate = RegenRate;
        // 世界已生成则同步时间轴，否则设为 0
        NewData.LastSyncGameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        NewData.bIsRegenFrozen = false;
    }
}

void USkillPointSubsystem::UpdateCharacterConfig(FName CharacterID, float NewMaxSP, float NewRegenRate)
{
    FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr) return;

    // 【关键时间对齐修正】
    // 在外界执行属性改变的一瞬间，我们必须先调用只读函数计算出“当前此刻”的精确点数
    float CurrentSP = GetCalculatedCharacterSP(CharacterID);

    // 强制将过去的数额“固化结算”放进底座，并将时间戳同步对齐到这一刻！
    DataPtr->SavedSP = CurrentSP;
    DataPtr->LastSyncGameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    // 过去的账目结清后，现在可以安全覆盖新上限和新恢复速度了！
    // 接下来流逝的时间差，将完全安全地按照新的速度执行，绝不污染过去的历史。
    DataPtr->MaxSP = NewMaxSP;
    DataPtr->RegenRate = NewRegenRate;

    // 【新增广播】：通知 UI，技能点上限改变，必须立刻呼叫 UI 蓝图重新拉取最新的快照进行重绘
    // ==========================================
    OnSPChanged.Broadcast(CharacterID, GetCharacterSPPercent(CharacterID));
}

void USkillPointSubsystem::RemoveCharacterFromSquad(FName CharacterID)
{
    // 哈希表原生的 Remove 极速擦除，内存干干净净
    SquadSPMap.Remove(CharacterID);
}
