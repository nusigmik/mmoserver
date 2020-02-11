#pragma once

#include "Common.h"
#include "RemoteClient.h"
#include "TypeDef.h"
#include "DBSchema.h"
#include "ClientInterestArea.h"

namespace PCS = ProtocolCS;
namespace db = db_schema;

class WorldServer;
class World;
class Hero;

// ���� ������ ������ ����Ʈ Ŭ���̾�Ʈ.
class RemoteWorldClient : public RemoteClient
{
public:
	enum class State
	{
		Connected,		// ������ �ʱ����. ���� ���� �ƴ�
		WorldEntering,	// ���� ���� ��
		WorldEntered,	// ���� ���� ��
		Disconnected,	// ���� ����
	};

	RemoteWorldClient(const Ptr<net::Session>& net_session, WorldServer* owner);
	virtual ~RemoteWorldClient();
    
    // ���� ó��. ���� DB Update �� �� �Ѵ�.
    void Dispose();
    bool IsDispose() const { return disposed_; }

    // ����
	State GetState() const { return state_; }
	void SetState(State state) { state_ = state; }

    // ���� ����
	const Ptr<db::Account>& GetAccount() const { return db_account_; }
	void SetAccount(Ptr<db::Account> db_account) { db_account_ = db_account; }

    // ������ ����
    void Authenticate(uuid credential) { credential_ = credential; }
    // ���� Ȯ��
	bool IsAuthenticated() const { return !credential_.is_nil(); }
    // ������
	const uuid& GetCredential() const
	{
		return credential_;
	}

    // ĳ���� DB ����
	const Ptr<db::Hero> GetDBHero()
	{
		return db_hero_;
	}
	void SetDBHero(const Ptr<db::Hero>& db_hero)
	{
		db_hero_ = db_hero;
	}

    // ĳ���� ��ü
	const Ptr<Hero>& GetHero()
	{
		return hero_;
	}

    // ���� ����
    World* GetWorld();
    
	// ���¸� DB�� ������Ʈ
    void UpdateToDB();
    const Ptr<MySQLPool>& GetDB();

	// Ŭ���̾�Ʈ���� ������ �������� callback
	virtual void OnDisconnected() override
	{
        RemoteClient::OnDisconnected();
        // ���� ó��
		Dispose();
	}
	
    // ���� ����
    void EnterWorld();
    // ���� ����
    void ExitWorld();
    // �̵�
    void ActionMove(const PCS::World::Request_ActionMove * message);
    // ��ų ���
    void ActionSkill(const PCS::World::Request_ActionSkill * message);
    // ��� ��Ȱ
    void RespawnImmediately();
    // �� �̵�
    void EnterGate(const PCS::World::Request_EnterGate * message);

    int selected_hero_uid_;

private:
    void EnterZone(const Ptr<Hero>& hero, int map_id, const Vector3& position, std::function<void(bool)> handler = nullptr);
    void ExitZone(const Ptr<Hero>& hero, std::function<void()> handler = nullptr);

    void Respawn(const Ptr<Hero> hero);

    // callback handler
    void OnUpdateHeroPosition(const Vector3& position)
    {
        if (interest_area_)
        {
            interest_area_->Position(position);
            interest_area_->UpdateInterest();
        }
    }

    void OnHeroDeath();

	std::atomic<bool>       disposed_;

	WorldServer*            owner_;
	Ptr<db::Account>        db_account_;
	uuid					credential_;
	
	std::atomic<State>		state_;
	Ptr<db::Hero>			db_hero_;
	Ptr<Hero>				hero_;
    Ptr<ClientInterestArea> interest_area_;

	time_point last_position_update_time_;
	time_point last_attack_time_;

    Ptr<timer_type> respawn_timer_;
};