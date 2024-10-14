#ifndef QUARKPUNK_PQXX_POOL_HPP
#define QUARKPUNK_PQXX_POOL_HPP

#include<pqxx/pqxx>
#include<memory>
#include<queue>
#include<mutex>
#include<atomic>
#include<future>

namespace pqxx{
class pool_connection;
class pool{
public:
    friend pool_connection;
    pool(const pool&) = delete;
    pool& operator=(const pool&) = delete;
    pool(const std::string& conn_string, size_t min_pool_size = 2, size_t max_pool_size = 5);
    ~pool();
    int size_connections();
    int size_connections_overhead();
    std::unique_ptr<pool_connection> acquire();
private:
    std::shared_ptr<pqxx::connection> create_connection();
    std::shared_ptr<pqxx::connection> get_connection();
    void release_connection(std::shared_ptr<pqxx::connection> conn);
    void close_connections_all();
    void async_shrink_start();
    void async_shrink_stop();
    void async_shrink_pool();

    size_t min_pool_size;
    size_t max_pool_size;
    size_t total_connections = 0;
    size_t overhead_pool_size = 0;
    int64_t timeout_shrink = 20;
    std::mutex mutex;
    std::string conn_string;
    std::future<void> future_shrink;
    std::atomic<bool> is_running;
    std::condition_variable cv;
    std::queue<std::shared_ptr<pqxx::connection>> connections;
};

class pool_connection {
public:
    pool_connection(const pool_connection&) = delete;
    pool_connection& operator=(const pool_connection&) = delete;
    pool_connection(std::shared_ptr<pqxx::connection> conn, pool& pool) : conn(conn), parent_pool(parent_pool){}
    ~pool_connection();
    pqxx::connection& pqxx();
    pqxx::result execute(const std::string& query);
private:
    std::shared_ptr<pqxx::connection> conn;
    pool& parent_pool;
};
};

#endif