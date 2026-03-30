// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

/**
胜负判定逻辑：
如果僵尸进入了房子（某个坐标），触发 GameOver()。
如果玩家清空了所有波次的僵尸，触发 LevelComplete()。

僵尸生成器（Spawner）：
管理僵尸在什么时候、哪个格子刷新。

资源管理：
阳光的自动掉落、收集和全局计数。

关卡阶段控制：
“第一波僵尸正在接近”、“一大波僵尸即将来袭”的倒计时。
*/