#pragma once
#include "Vector3.h"

using namespace AO::Vector3;

struct BoundingBox
{
    Vector3 min;
    Vector3 max;

    BoundingBox(const Vector3& min, const Vector3& max)
        : min(min), max(max)
    {
    }

    Vector3 Size() const
    {
        return max - min; 
    }

    bool Contains(const Vector3& point) const
    {
        // not outside of box?
        return (point.X < min.X || point.X > max.X || point.Y < min.Y || point.Y > max.Y || point.Z < min.Z || point.Z > max.Z) ==
            false;
    }

    bool Contains2d(const Vector3& point) const
    {
        // not outside of box?
        return (point.X < min.X || point.X > max.X || point.Y < min.Y || point.Y > max.Y) ==
            false;
    }
    // 겹치는 영역
    BoundingBox IntersectWith(const BoundingBox& other) const
    {
        return BoundingBox(Max(min, other.min), Min(max, other.max));
    }
    // 두영역을 포함하는 영역
    BoundingBox UnionWith(const BoundingBox& other) const
    {
        return BoundingBox(Min(min, other.min), Max(max, other.max));
    }

    bool IsValid() const
    {
        return (max.X < min.X || max.Y < min.Y || max.Z < min.Z) == false;
    }

    friend std::ostream& operator<< (std::ostream& os, const BoundingBox& rhs);
};

//Output display of values
inline std::ostream& operator<< (std::ostream& os, const BoundingBox& rhs)
{
    os << "(" << rhs.min << ")(" << rhs.max << ")";
    return os;
}