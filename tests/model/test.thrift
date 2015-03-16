namespace cpp test

/// a traditional thrift service
service returning_service
{
    i32 add(1:i32 a, 2:i32 b);
}

