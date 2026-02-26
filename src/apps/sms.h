#pragma once

#include "vos/types.h"
#include <string>
#include <vector>
#include <deque>
#include <functional>

namespace vos {

// ─── Message Record ──────────────────────────────────────────
struct ChatMessage {
    std::string peer_id;
    std::string text;
    TimePoint   timestamp;
    bool        outgoing;   // true = sent by us
    bool        delivered;
};

// ─── Conversation ────────────────────────────────────────────
struct Conversation {
    std::string               peer_id;
    std::deque<ChatMessage>   messages;    // Ordered oldest→newest
    TimePoint                 last_activity;
    int                       unread_count = 0;
};

using NewMessageFn = std::function<void(const ChatMessage&)>;

// ─── SMS App ─────────────────────────────────────────────────
class SmsApp {
public:
    SmsApp();
    ~SmsApp() = default;

    Result<void> init();

    // Send a text message to a peer
    Result<void> send(const std::string& peer_id, const std::string& text);

    // Receive a message (called by mesh_net callback)
    void receive(const std::string& peer_id, const std::string& text);

    // Get all conversations
    const std::vector<Conversation>& get_conversations() const { return m_conversations; }

    // Get conversation for a specific peer
    Conversation* get_conversation(const std::string& peer_id);

    // Mark conversation as read
    void mark_read(const std::string& peer_id);

    // Total unread messages
    int total_unread() const;

    // Register callback
    void on_new_message(NewMessageFn fn);

private:
    Conversation& get_or_create(const std::string& peer_id);

    std::vector<Conversation>    m_conversations;
    std::vector<NewMessageFn>    m_callbacks;
};

} // namespace vos
