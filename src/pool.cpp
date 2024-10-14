#include<pqxx/pool.hpp>
#include<iostream>
#include<chrono>

namespace pqxx{

pool::pool(const std::string& conn_string, size_t min_pool_size, size_t max_pool_size){
    this->conn_string = conn_string;
    this->min_pool_size = min_pool_size;
    this->max_pool_size = max_pool_size;
    if((min_pool_size <= 0 || max_pool_size <= 0) || (min_pool_size > max_pool_size)){
        this->min_pool_size = 3;
        this->max_pool_size = 7;
        printf("pqxx-pool: (warning) bad configuration: min_pool_size=%lu | max_pool_size=%lu\n", min_pool_size, max_pool_size);
        printf("pqxx-pool: used default configuratuin: min_pool_size=%lu | max_pool_size=%lu\n", this->min_pool_size, this->max_pool_size);
    }
    try {
        for(size_t i = 0; i < this->min_pool_size; ++i){
            auto conn = create_connection();
            if(conn){
                connections.push(conn);
                total_connections++;
            }
        }
        auto conn = get_connection();
        bool status = conn && conn->is_open();
        status  ? printf("pqxx-pool: connection success to %s, pool-size: %lu\n", conn->hostname(), connections.size())
                : printf("pqxx-pool: failed connect to %s\n", conn->hostname());
        release_connection(conn);
        async_shrink_start();
    }
    catch(const std::exception& e){
        std::cerr << e.what() << '\n';
    }
}

pool::~pool(){
    async_shrink_stop();
    close_connections_all();
}

int pool::size_connections(){
    return total_connections - overhead_pool_size;
}

int pool::size_connections_overhead(){
    return overhead_pool_size;
}

// get a free connection from the pool if all free connections in the pool are missing
// a new connection will be created, which will then be automatically released by the pool
// the connection returned automatically after the connection object goes out of scope range
std::unique_ptr<pool_connection> pool::acquire(){
    return std::make_unique<pool_connection>(get_connection(), *this);
}

std::shared_ptr<pqxx::connection> pool::get_connection(){
    std::unique_lock<std::mutex> lock(mutex);
    if(connections.empty()) {
        auto new_conn = create_connection();
        if(new_conn){
            total_connections++;
            if(total_connections > max_pool_size) overhead_pool_size++;
            return new_conn;
        }
    }
    else{
        auto conn = connections.front();
        connections.pop();
        return conn;
    }
    throw std::runtime_error("pqxx-pool: failed get new connection");
}

void pool::async_shrink_start(){
    is_running = true;
    future_shrink = std::async(std::launch::async, &pool::async_shrink_pool, this);
}

void pool::async_shrink_stop(){
    {
        std::unique_lock<std::mutex> lock(mutex);
        is_running = false;
    }
    cv.notify_one();
    future_shrink.wait();
}

void pool::async_shrink_pool() {
    while(is_running){
        std::unique_lock<std::mutex> lock(mutex);
        if(cv.wait_for(lock, std::chrono::seconds(timeout_shrink), [this] { return !is_running; })){
            break;
        }
        if(connections.size() > min_pool_size){
            auto conn = connections.front();
            connections.pop();
            if(conn->is_open()){
                conn->close();
                total_connections--;
            }
        }
    }
}

void pool::release_connection(std::shared_ptr<pqxx::connection> conn) {
    std::unique_lock<std::mutex> lock(mutex);
    if(overhead_pool_size > 0){
        overhead_pool_size--;
        conn->close();
        total_connections--;
        return;
    }
    if(connections.empty()){
        if(!conn || !conn->is_open()) conn = create_connection();
        connections.push(conn);
    }
    else{
        if(connections.size() + 1 <= max_pool_size){
            if(!conn || !conn->is_open()) conn = create_connection();
            connections.push(conn);
            return;
        }
        conn->close();
        total_connections--;
    }
}

void pool::close_connections_all() {
    std::unique_lock<std::mutex> lock(mutex);
    while(!connections.empty()){
        auto conn = connections.front();
        connections.pop();
        if (conn->is_open()){
            conn->close();
        }
    }
    total_connections = 0;
    overhead_pool_size = 0;
}

std::shared_ptr<pqxx::connection> pool::create_connection() {
    try{
        return std::make_shared<pqxx::connection>(conn_string);
    }catch (const std::exception& e) {
        std::cerr << "pqxx-pool: failed create new connection: " << e.what() << std::endl;
        return nullptr;
    }
}

pool_connection::~pool_connection(){
    parent_pool.release_connection(conn);
}

// get original pqxx connection
// - return pqxx::connection
pqxx::connection& pool_connection::pqxx(){
    return *conn;
}

// fast execute sql-query and commmit
// - return pqxx::result
pqxx::result pool_connection::execute(const std::string& query) {
    pqxx::work txn(pqxx());
    try {
        pqxx::result result = txn.exec(query);
        txn.commit();
        return result;
    } catch (const std::exception& e) {
        txn.abort();
        std::cerr << "pqxx-pool: " << e.what() << std::endl;
        return pqxx::result();
    }
}

};