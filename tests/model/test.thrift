namespace cpp test

/// a traditional thrift service
service synchronous_service
{
    i32 add(1:i32 a, 2:i32 b);
}

/// a server that's async. i.e. all functions are oneway
service asynchronous_server
{
    oneway void add(1:i32 a, 2:i32 b);
}

/// and the matching async client
service asynchronous_client
{
    oneway void on_added(1:i32 result);
}