#pragma once
#include <unordered_set>
#include <boost/signals2.hpp>
#include "Common.h"

using namespace boost;

class Actor;
class Zone;
class ZoneCell;

// ���� ����
class InterestArea
{
public:
    using GridType = Zone;
    using CellType = ZoneCell;

    InterestArea(GridType* grid);
    ~InterestArea();

    // ���� ������ ��ǥ
    const Vector3& Position() const { return position_; }
    void Position(const Vector3& value) { position_ = value; }
    // ���� ������ �Ÿ�
    const Vector3& ViewDistance() const { return view_distance_; }
    const void ViewDistance(const Vector3& value) { view_distance_ = value; }

    // ���� ������ ����
    void UpdateInterest();

    // ���� ������ Actor�� ���ö�
    virtual void OnActorEnter(Actor*) {}
    // ���� ������ Actor�� ������
    virtual void OnActorExit(Actor*) {}

protected:
    // ���� ������ Cell�� ���ö�
    virtual void OnCellEnter(CellType* cell);
    // ���� ������ Cell�� ������
    virtual void OnCellExit(CellType* cell);

private:
    // Actor�� ���� Cell�� �����ų� ������
    void OnActorCellChange(CellType* exit, CellType* enter, Actor* actor);

    GridType* grid_;
    // Cells in the area
    std::vector<CellType*> cells_;
    std::map<CellType*, signals2::connection> connections_;

    Vector3 position_;
    Vector3 view_distance_;
};