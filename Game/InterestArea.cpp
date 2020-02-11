#include "stdafx.h"
#include "InterestArea.h"
#include "Zone.h"

InterestArea::InterestArea(GridType * grid)
    : grid_(grid)
{
}

InterestArea::~InterestArea()
{
    // 구독 해제
    for (auto& e : connections_)
    {
        e.second.disconnect();
    }
}

void InterestArea::UpdateInterest()
{
    // 관심 영역의 구역을 얻어온다.
    BoundingBox focus(Position() - ViewDistance(), Position() + ViewDistance());
    std::vector<CellType*> new_cells = grid_->GetCells(focus);
    std::sort(new_cells.begin(), new_cells.end());
    if (new_cells == cells_)
        return;

    std::vector<CellType*> result;
    // 새로운 구역 구하기
    std::set_difference(new_cells.begin(), new_cells.end(), cells_.begin(), cells_.end(), std::back_inserter(result));
        
    for (CellType* cell : result)
    {
        // 새 구역을 구독 및 등록
        OnCellEnter(cell);
        // interest area 가 cell 에 들어왔음을 알린다.
        cell->interest_area_enter_signal(this);
    }

    result.clear();
    // 나간 구역 구하기
    std::set_difference(cells_.begin(), cells_.end(), new_cells.begin(), new_cells.end(), std::back_inserter(result));
    for (CellType* cell : result)
    {
        // 나간 구역 구독 해제 및 삭제
        OnCellExit(cell);
        // interest area 가 cell 에 나감을 알린다.
        cell->interest_area_exit_signal(this);
    }

    cells_.swap(new_cells);
}

void InterestArea::OnCellEnter(CellType * cell)
{
    // 새 구역을 구독 및 등록
    signals2::connection conn = cell->actor_cell_change_signal.connect(std::bind(&InterestArea::OnActorCellChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    connections_.emplace(cell, std::move(conn));
}

void InterestArea::OnCellExit(CellType * cell)
{
    // 나간 구역 구독 해제 및 삭제
    auto iter = connections_.find(cell);
    if (iter != connections_.end())
    {
        iter->second.disconnect();
        connections_.erase(iter);
    }
}

void InterestArea::OnActorCellChange(CellType * exit, CellType * enter, Actor * actor)
{
    auto iter1 = std::find(cells_.begin(), cells_.end(), exit);
    auto iter2 = std::find(cells_.begin(), cells_.end(), enter);
    if (iter1 != cells_.end() && iter2 == cells_.end())
    {
        OnActorExit(actor);
    }
    else if (iter1 == cells_.end() && iter2 != cells_.end())
    {
        OnActorEnter(actor);
    }
}
