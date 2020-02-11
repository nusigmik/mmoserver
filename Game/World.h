#pragma once
#include "Common.h"
#include "DBSchema.h"
#include "Zone.h"
#include "InstanceZone.h"
#include <boost\multi_index_container.hpp>
#include <boost\multi_index\hashed_index.hpp>
#include <boost\multi_index\member.hpp>
#include <boost\multi_index\mem_fun.hpp>

using namespace boost::multi_index;

// 태그선언
struct zone_tags
{
	struct entity_id {};
	struct map_id {};
};
// 인덱싱 타입을 선언
using indices = indexed_by<
	hashed_unique<
		tag<zone_tags::entity_id>, const_mem_fun<Zone, const uuid&, &Zone::EntityId>, boost::hash<boost::uuids::uuid>
	>,
	hashed_non_unique<
		tag<zone_tags::map_id>, const_mem_fun<Zone, int, &Zone::MapId>
	>
>;
// 존 컨테이너 타입 선언
using ZoneSet = boost::multi_index_container<Ptr<Zone>, indices>;

// 게임의 시뮬레이션.
// 게임 객체를 관리.
// 캐릭터, 몬스터, 지역 생성 및 삭제.
class World : public std::enable_shared_from_this<World>
{
public:
	World(const World&) = delete;
	World& operator=(const World&) = delete;

	World(Ptr<EventLoop>& loop);
	~World();

    // 게임 월드 시작.
	void Start();
    // 게임 월드 종료.
	void Stop();

    // 직렬화 객체.
	strand& GetStrand() { return strand_; }
	
    // 필드 존 객체를 얻는다.
    Zone* FindFieldZone(int map_id);
    // 인스턴스 존 객체를 얻는다.
    InstanceZone* FindInstanceZone(const uuid& entity_id);
	
	// 직렬화 작업 실행.
	template <typename Handler>
	void Dispatch(Handler&& handler)
	{
		strand_.dispatch(std::forward<Handler>(handler));
	}

    // 타이머 예약.
    template <typename Handler>
    Ptr<timer_type> RunAt(time_point time, Handler&& handler)
    {
        Ptr<timer_type> timer = std::make_shared<timer_type>(ev_loop_->GetIoContext(), time);
        timer->async_wait(strand_.wrap([timer, handler = std::forward<Handler>(handler)](const auto& error)
        {
            if (!error)
            {
                if (handler) handler(timer);
            }
            else
            {
                BOOST_LOG_TRIVIAL(info) << error;
            }
        }));
        return timer;
    }

    // 타이머 예약.
    template <typename Handler>
    Ptr<timer_type> RunAfter(duration duration, Handler&& handler)
    {
        Ptr<timer_type> timer = std::make_shared<timer_type>(ev_loop_->GetIoContext(), duration);
        timer->async_wait(strand_.wrap([timer, handler = std::forward<Handler>(handler)](const boost::system::error_code& error)
        {
            if (!error)
            {
                handler(timer);
            }
            else
            {
                BOOST_LOG_TRIVIAL(info) << error;
            }
        }));
        return timer;
    }

    // 인스턴스 존 을 만든다.
    InstanceZone* CreateInstanceZone(int map_id);
    // 존을 삭제한다.
    bool DeleteZone(const uuid& entity_id)
    {
        auto& indexer = zone_set_.get<zone_tags::entity_id>();
        auto iter = indexer.find(entity_id);
        if (iter == indexer.end())
            return false;

        zone_set_.erase(iter);
        return true;
    }

    // 프레임 업데이트.
	void DoUpdate(float delta_time);

private:
	void CreateFieldZones();

    Ptr<EventLoop> ev_loop_;
	strand strand_;
	ZoneSet zone_set_;
};


