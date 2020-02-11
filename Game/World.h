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

// �±׼���
struct zone_tags
{
	struct entity_id {};
	struct map_id {};
};
// �ε��� Ÿ���� ����
using indices = indexed_by<
	hashed_unique<
		tag<zone_tags::entity_id>, const_mem_fun<Zone, const uuid&, &Zone::EntityId>, boost::hash<boost::uuids::uuid>
	>,
	hashed_non_unique<
		tag<zone_tags::map_id>, const_mem_fun<Zone, int, &Zone::MapId>
	>
>;
// �� �����̳� Ÿ�� ����
using ZoneSet = boost::multi_index_container<Ptr<Zone>, indices>;

// ������ �ùķ��̼�.
// ���� ��ü�� ����.
// ĳ����, ����, ���� ���� �� ����.
class World : public std::enable_shared_from_this<World>
{
public:
	World(const World&) = delete;
	World& operator=(const World&) = delete;

	World(Ptr<EventLoop>& loop);
	~World();

    // ���� ���� ����.
	void Start();
    // ���� ���� ����.
	void Stop();

    // ����ȭ ��ü.
	strand& GetStrand() { return strand_; }
	
    // �ʵ� �� ��ü�� ��´�.
    Zone* FindFieldZone(int map_id);
    // �ν��Ͻ� �� ��ü�� ��´�.
    InstanceZone* FindInstanceZone(const uuid& entity_id);
	
	// ����ȭ �۾� ����.
	template <typename Handler>
	void Dispatch(Handler&& handler)
	{
		strand_.dispatch(std::forward<Handler>(handler));
	}

    // Ÿ�̸� ����.
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

    // Ÿ�̸� ����.
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

    // �ν��Ͻ� �� �� �����.
    InstanceZone* CreateInstanceZone(int map_id);
    // ���� �����Ѵ�.
    bool DeleteZone(const uuid& entity_id)
    {
        auto& indexer = zone_set_.get<zone_tags::entity_id>();
        auto iter = indexer.find(entity_id);
        if (iter == indexer.end())
            return false;

        zone_set_.erase(iter);
        return true;
    }

    // ������ ������Ʈ.
	void DoUpdate(float delta_time);

private:
	void CreateFieldZones();

    Ptr<EventLoop> ev_loop_;
	strand strand_;
	ZoneSet zone_set_;
};


