#pragma once
#include "Common.h"
#include "RemoteClient.h"
#include "DBSchema.h"

namespace fb = flatbuffers;
namespace db = db_schema;


// 로그인 서버에 접속한 리모트 클라이언트.
class RemoteLoginClient : public RemoteClient
{
public:
	RemoteLoginClient(const RemoteLoginClient&) = delete;
	RemoteLoginClient& operator=(const RemoteLoginClient&) = delete;

	RemoteLoginClient(const Ptr<net::Session>& net_session, Ptr<db::Account> db_account)
		: RemoteClient(net_session)
		, db_account_(db_account)
		, disposed_(false)
	{
		assert(net_session);
		assert(net_session->IsOpen());
		assert(db_account);
	}

	~RemoteLoginClient()
	{
		Dispose();
	}

    // 종료 처리. 상태 DB Update 등 을 한다.
    void Dispose()
    {
        bool exp = false;
        if (!disposed_.compare_exchange_strong(exp, true))
            return;
    }
	bool IsDisposed() const
	{
		return disposed_;
	}

    // 계정 정보
	const Ptr<db::Account>& GetAccount() const
	{
		return db_account_;
	}

    // 인증서 저장
    void Authenticate(uuid credential)
    {
        credential_ = credential;
    }
    // 인증 확인
	bool IsAuthenticated() const
	{
		return !credential_.is_nil();
	}
    // 인증서
	const uuid& GetCredential() const
	{
		return credential_;
	}

    // 클라이언트에서 연결을 끊었을때 callback
	virtual void OnDisconnected() override
	{
		Dispose();
	}

private:
	std::atomic<bool>       disposed_;
	Ptr<db::Account>		db_account_;

	uuid					credential_;
};
