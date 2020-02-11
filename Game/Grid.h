#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "BoundingBox.h"

class BasicCell
{
public:
    BasicCell(int x, int y, int z)
        : x_(x), y_(y), z_(z) {}
    virtual ~BasicCell() {}

    int X() { return x_; }
    int Y() { return y_; }
    int Z() { return z_; }

private:
    int x_;
    int y_;
    int z_;
};

template <typename T = BasicCell>
class Grid
{
public:
	using CellType = T;

	Grid(BoundingBox area, Vector3 tileDimensions)
		: area_(area), tileDimensions_(tileDimensions)
	{
		tile_x_ = (int)std::ceil(area_.Size().X / (double)tileDimensions_.X);
		tile_y_ = (int)std::ceil(area_.Size().Y / (double)tileDimensions_.Y);
        tile_z_ = (int)std::ceil(area_.Size().Z / (double)tileDimensions_.Z);

        for (int x = 0; x < tile_x_; x++)
        {
            cells_.emplace_back(std::vector<std::vector<CellType>>());
            for (int y = 0; y < tile_y_; y++)
            {
                cells_[x].emplace_back(std::vector<CellType>());
                for (int z = 0; z < tile_z_; z++)
                {
                    cells_[x][y].emplace_back(CellType(x, y, z));
                }
            }
        }
	}
	virtual ~Grid() {}

    const BoundingBox& Area() const { return area_; }
    const Vector3& TileDimensions() const { return tileDimensions_; }
    int TileX() const { return tile_x_; }
    int TileY() const { return tile_y_; }
    int TileZ() const { return tile_z_; }

	CellType* GetCell(int tile_x, int tile_y, int tile_z)
	{
        if (tile_x >= 0 && tile_x < tile_x_ &&
            tile_y >= 0 && tile_y < tile_y_ &&
            tile_z >= 0 && tile_z < tile_z_)
        {
            return &(cells_[tile_x][tile_y][tile_z]);
        }
        else
        {
            return nullptr;
        }
	}
	
	CellType* GetCell(const Vector3& position)
	{
        Vector3 p = position - area_.min;
        if (p.X >= 0 && p.X < area_.Size().X &&
            p.Y >= 0 && p.Y < area_.Size().Y &&
            p.Z >= 0 && p.Z < area_.Size().Z)
        {
            return GetCell((int)(p.X / tileDimensions_.X), (int)(p.Y / tileDimensions_.Y), (int)(p.Z / tileDimensions_.Z));
        }
        else
        {
            return nullptr;
        }
	}

	std::vector<CellType*> GetCells(const BoundingBox& area)
	{
        BoundingBox overlap = area_.IntersectWith(area);
        auto min = overlap.min - area_.min;
        auto max = overlap.max - area_.min;
        
        // 타일 좌표로 변환
        int x0 = std::max((int)(min.X / tileDimensions_.X), 0);
        int x1 = std::min((int)std::ceil(max.X / tileDimensions_.X), tile_x_);

        int y0 = std::max((int)(min.Y / tileDimensions_.Y), 0);
        int y1 = std::min((int)std::ceil(max.Y / tileDimensions_.Y), tile_y_);

        int z0 = std::max((int)(min.Z / tileDimensions_.Z), 0);
        int z1 = std::min((int)std::ceil(max.Z / tileDimensions_.Z), tile_z_);

        std::vector<CellType*> result;
        for (int x = x0; x < x1; x++)
        {
            for (int y = y0; y < y1; y++)
            {
                for (int z = z0; z < z1; z++)
                {
                    result.push_back(&(cells_[x][y][z]));
                }
            }
        }
		return std::move(result);
	}

private:
    std::vector<std::vector<std::vector<CellType>>> cells_;
    BoundingBox area_;
    Vector3 tileDimensions_;
	int tile_x_;
	int tile_y_;
    int tile_z_;
};