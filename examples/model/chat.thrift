namespace cpp example.chat

/*!
 *  The chat server
 *  ===============
 *  We call it yell-chat, because it's only possible to broadcast messages
 */
service chat_server
{
    /// The client is expected to call this on connect
    oneway void set_user_name(1:string name);

    /// broadcast message to all connected clients.
    oneway void broadcast_message(1:string message);
}

/*!
 * The Chat client.
 */
service chat_client
{
    /// called, if claiming the desired user name is not possible
    oneway void on_set_user_name_failed(1:string why);

    oneway void on_set_user_name_succeeded();

    /// called, when a chat-message is received
    oneway void on_message(1:string from_user, 2:string message);

    /// called, if sending a message has failed (e.g. if to_user is unknown)
    oneway void on_send_message_failed(1:string why);
}
