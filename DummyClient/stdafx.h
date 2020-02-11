// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include <iostream>
#include <chrono>
#include <type_traits>
#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <tuple>
#include <random>
#include <boost/signals2.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/clamp.hpp>
#include <network.h>
#include <flatbuffers\flatbuffers.h>
#include <protocol_cs_generated.h>
#include <protocol_ss_generated.h>
#include "Vector3.h"

using namespace net;

template<typename T>
using UPtr = std::unique_ptr<T>;

using namespace std::chrono_literals;

// Update frame time step.(20fps)
constexpr std::chrono::nanoseconds TIME_STEP(50ms);

using double_seconds = std::chrono::duration<double>;

// The clock type.
using clock_type = std::chrono::high_resolution_clock;
// The duration type of the clock.
using duration = clock_type::duration;
// The time point type of the clock.
using time_point = clock_type::time_point;
// Timer type
using timer_type = boost::asio::high_resolution_timer;

using boost::asio::strand;
using boost::uuids::uuid;
namespace std
{
    template<>
    struct hash<boost::uuids::uuid>
    {
        size_t operator () (const boost::uuids::uuid& uid) const
        {
            return boost::hash<boost::uuids::uuid>()(uid);
        }
    };
}
using uuid_hasher = std::hash<boost::uuids::uuid>;

using namespace AO::Vector3;

constexpr double rad2deg = 180 / std::_Pi;
constexpr double deg2rad = std::_Pi / 180;