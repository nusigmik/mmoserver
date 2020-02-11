#pragma once

// 영웅 클래스 타입
enum class ClassType : int
{
	NONE = 0,
	Knight = 1,
	Archer = 2,
	Mage = 3,
};

// 맵 타입
enum class MapType : int
{
	NONE = 0,
	Field = 1,
	Dungeon = 2,
};

// 타겟팅 타입
enum class TargetingType : int
{
    NONE = 0,
    Self,
    One,
    Around,
};