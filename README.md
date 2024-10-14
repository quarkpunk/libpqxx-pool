# PQXX Connection Pool
PQXX Connection Pool is a small library for managing a pool of PostgreSQL database connections using the pqxx library. It provides efficient connection reuse by preventing redundant creation of new connections and automatically returning them to the pool after use

## Features
- Connection Management: Create and reuse connections until the maximum pool size is reached
- Automatic Connection Return: Connections are automatically returned to the pool after use
- Asynchronous Pool Decrease: Gradually decrease the number of connections to the minimum value
- Monitoring: Get information about the current number of connections and additional connections created in excess of the maximum pool size

## Dependencies:
- C++ 17
- cmake
- pqxx (libpqxx)

## Still need
- Tests
- Minor fixes logic
- Conan package

## Usage
The usage is very simple: you get a connection from the pool `acquire` and use it, you don't need to close the connection, the pool will close it itself when the connection you created goes out of the function scope
```cpp
// creating pool
pqxx::pool pool("pqxx_connection_string", 3, 5);
// get connection from pool
auto conn = pool.acquire();
// fast execute pqxx sql-query with commit 
pqxx::result result = conn->execute("SELECT * FROM table");
// or use original pqxx connection
conn->pqxx().dbname();
```
```cpp
auto conn = pool.acquire();
auto result = conn->execute("SELECT * FROM quarkpunk.users");
for(const auto& row : result){
    printf("user-id: %s-%s-%s\n", 
        row["id"].as<std::string>().c_str(),
        row["name"].as<std::string>().c_str(),
        row["city"].as<std::string>().c_str()
    );
}
```

You can also find out the number of connections of 2 types:
```cpp
// return number default pqxx connections
pool.size_connections();
// return number overhead pqxx connections
pool.size_connections_overhead();
```

## What is pool
Database connection pool - it helps save resources, speeds up the application and makes it more stable, the connection pool allows you to reuse already created connections, which reduces the load on the system and speeds up the processing of requests, and also helps control the number of simultaneously open connections, preventing overload

## Pool blocking thread?
Nope, connection pool does not block the main execution thread, you can safely use it on your backend or for other purposes

## What is overhead connections?
Over head connections - these are connections that are created beyond `max_pool_size` if there are no free connections available at the moment, this is necessary to guarantee a new connection and not create waits and timeouts. Yes, the number of connections to postgresql will be higher than `max_pool_size`, but all these connections will be guaranteed to be destroyed after their use

## Auto pool reduction?
Yes, the pool is automatically reduced if the number of connections is higher than `min_pool_size`, the pool gradually begins to reduce the number of connections with a periodicity of 15 seconds thanks to the asynchronous timer inside it, thus, the pool tends to `min_pool_size`

## License
Do with it what you want, I don't care, I made it for myself, you can use it anywhere and however you want