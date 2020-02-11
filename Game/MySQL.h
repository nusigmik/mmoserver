#pragma once

#include <memory>
#include <mutex>
#include <future>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using ConnectionPtr = std::unique_ptr<sql::Connection, std::function<void(sql::Connection*)>>;
using StmtPtr = std::unique_ptr<sql::Statement>;
using PstmtPtr = std::unique_ptr<sql::PreparedStatement>;
using ResultSetPtr = std::shared_ptr<sql::ResultSet>;

// MySQL database connection pool
class MySQLPool : public std::enable_shared_from_this<MySQLPool>
{
public:
	~MySQLPool()
	{
		Finalize();
	}

	static std::shared_ptr<MySQLPool> Create(
		const std::string& url,
		const std::string& user,
		const std::string& password,
		const std::string& database,
		size_t connection_count = 1,
		const std::string& connection_charset = "")
	{
		return std::make_shared<MySQLPool>(url, user, password, database, connection_count, connection_charset);
	}

	// 풀에서 커넥션을 가져온다.
	ConnectionPtr GetConnection()
	{
		std::lock_guard<std::mutex> guard(mutex_);
		if (pool_list_.empty())
		{
			sql::Connection* conn = CreateConnection();
			connection_pool_.push_back(conn);
			pool_list_.push_front(conn);
		}

		sql::Connection* conn = pool_list_.front();
		pool_list_.pop_front();

		auto self = shared_from_this();
		return ConnectionPtr(conn, [this, self](sql::Connection* ptr) { ReleaseConnection(ptr); });
	}

	// 비동기로 쿼리를 실행
	std::future<ResultSetPtr> ExcuteAsync(const std::string& query)
	{
		auto self = shared_from_this();
		return std::async(std::launch::async, [this, self, query] {
				ConnectionPtr conn = GetConnection();
				StmtPtr stmt(conn->createStatement());
				ResultSetPtr result;

				if (stmt->execute(query.c_str()))
					result.reset(stmt->getResultSet());

				return result;
			});
	}

	// 동기로 쿼리를 실행
	ResultSetPtr Excute(const std::string& query)
	{
		ConnectionPtr conn = GetConnection();
		StmtPtr stmt(conn->createStatement());
		ResultSetPtr result;
		
		if (stmt->execute(query.c_str()))
			result.reset(stmt->getResultSet());

		return result;
	}

	MySQLPool(const std::string& url, const std::string& user, const std::string& password,
		const std::string& database, size_t connection_count = 1,
		const std::string& connection_charset = "")
		: url_(url), user_(user), password_(password)
		, database_(database), connection_count_(connection_count)
		, connection_charset_(connection_charset)
	{
		Initialize();
	}

private:
	void Initialize()
	{
		driver_ = get_driver_instance();
		if (driver_ == nullptr)
			return;

		std::lock_guard<std::mutex> guard(mutex_);
		for (size_t i = 0; i < connection_count_; i++)
		{
			sql::Connection* conn = CreateConnection();
			connection_pool_.push_back(conn);
			pool_list_.push_front(conn);
		}
	}

	void Finalize()
	{
		std::lock_guard<std::mutex> guard(mutex_);
		for (sql::Connection* conn : connection_pool_)
		{
			delete conn;
		}
		connection_pool_.clear();
		pool_list_.clear();
	}

	sql::Connection* CreateConnection()
	{
		sql::Connection* conn = driver_->connect(url_.c_str(), user_.c_str(), password_.c_str());
		conn->setSchema(database_.c_str());
		conn->setClientOption("heroSetResults", connection_charset_.c_str());
		bool reconnect = true;
		conn->setClientOption("OPT_RECONNECT", (void*)&reconnect);

		return conn;
	}

	// 커넥션을 풀로 되돌린다.
	void ReleaseConnection(sql::Connection* conn)
	{
		std::lock_guard<std::mutex> guard(mutex_);
		pool_list_.push_front(conn);
	}
	
	std::string url_;
	std::string user_;
	std::string password_;
	std::string database_;
	size_t connection_count_;
	std::string connection_charset_;
	
	std::mutex mutex_;
	sql::Driver* driver_;
	std::vector<sql::Connection*> connection_pool_;
	std::list<sql::Connection*> pool_list_;
};