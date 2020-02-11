#pragma once
#include <unordered_set>
#include <boost/signals2.hpp>
#include "Common.h"

using namespace boost;

class Actor;
class Zone;
class ZoneCell;

// 관심 지역
class InterestArea
{
public:
    using GridType = Zone;
    using CellType = ZoneCell;

    InterestArea(GridType* grid);
    ~InterestArea();

    // 관심 지역의 좌표
    const Vector3& Position() const { return position_; }
    void Position(const Vector3& value) { position_ = value; }
    // 관심 지역의 거리
    const Vector3& ViewDistance() const { return view_distance_; }
    const void ViewDistance(const Vector3& value) { view_distance_ = value; }

    // 관심 지역을 갱신
    void UpdateInterest();

    // 관심 지역에 Actor가 들어올때
    virtual void OnActorEnter(Actor*) {}
    // 관심 지역에 Actor가 나갈때
    virtual void OnActorExit(Actor*) {}

protected:
    // 관심 지역에 Cell이 들어올때
    virtual void OnCellEnter(CellType* cell);
    // 관심 지역에 Cell이 나갈때
    virtual void OnCellExit(CellType* cell);

private:
    // Actor가 관심 Cell에 들어오거나 나갈때
    void OnActorCellChange(CellType* exit, CellType* enter, Actor* actor);

    GridType* grid_;
    // Cells in the area
    std::vector<CellType*> cells_;
    std::map<CellType*, signals2::connection> connections_;

    Vector3 position_;
    Vector3 view_distance_;
};