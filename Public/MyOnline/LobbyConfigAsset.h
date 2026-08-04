#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LobbyConfigAsset.generated.h"

// 【新增】：自己包装一个供虚幻编辑器面板使用的门禁枚举
// 【修改】：严格对齐 OSSv2 的底层 ELobbyJoinPolicy
UENUM(BlueprintType)
enum class EMyLobbyJoinPolicy : uint8
{
	PublicAdvertised    UMETA(DisplayName = "公开大厅 (全网可通过条件搜索)"),
	PublicNotAdvertised UMETA(DisplayName = "隐藏大厅 (不可搜索，仅凭大厅ID或邀请)"),
	InvitationOnly      UMETA(DisplayName = "私密大厅 (仅限邀请)")
};

/**
 * 大厅配置数据资产
 * 策划可以在编辑器中右键创建此资产，实时修改大厅容量、权限等核心骨架
 */
UCLASS(BlueprintType)
class CCC_API ULobbyConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 1. 核心参数：大厅最大人数 (EOS 支持最大 64 人)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings", meta = (ClampMin = "2", ClampMax = "64"))
	int32 MaxMembers = 4;

	// 2. 核心参数：门禁策略 (公开/好友/私密)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	EMyLobbyJoinPolicy JoinPolicy = EMyLobbyJoinPolicy::PublicAdvertised;

	// 3. 核心参数：社交平台曝光度 (是否允许 Epic 覆盖层显示玩家正在大厅中)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bPresenceEnabled = true;

	// =========================================================
	// 4. 核心参数：大厅注册表模板名 (SchemaId)
	// 【硬核白话解释】：
	// Epic 服务器不接受随便乱丢的数据。在你的 DefaultEngine.ini 里，
	// 必须有一段 [OnlineServices.EOS.Lobbies] 的配置，里面定义了一个叫 "GameLobby" 的模板（Schema）。
	// 这个模板规定了：“使用这个模板的大厅，允许附带哪些属性标签，哪些标签可以被玩家检索”。
	// 简单来说：这就是告诉 Epic 服务器：“我要用第几套数据结构标准来建这个房”。
	// =========================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	FName SchemaId = TEXT("GameLobby");

	// =========================================================
	// 5. 核心参数：业务层自定义的房间标签 (DefaultRoomType)
	// 【硬核白话解释】：
	// SchemaId 只是图纸，而这个参数才是真正的“数据”。
	// 试想你的游戏有“PVE剧情”、“PVP乱斗”、“交易市场”三种模式。
	// 建房时，你可以把这个值设为 "PVE_Story" 并作为一个 Attribute(属性) 贴在大厅上。
	// 当别的玩家（客机）在搜房时，就可以用这个标签作为过滤条件 (Filter) ：“我只要搜 PVE_Story 的房间”。
	// 这样就做到了零硬编码的自定义房间匹配！
	// =========================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby Attributes")
	FString DefaultRoomType = TEXT("PVE_Story");
};