#include "sms.h"
#include "vos/log.h"
#include <algorithm>

namespace vos {

static const char* TAG = "SMS";

SmsApp::SmsApp() = default;

Result<void> SmsApp::init() {
    log::info(TAG, "SMS app initialized");
    return Result<void>::success();
}

Result<void> SmsApp::send(const std::string& peer_id, const std::string& text) {
    if (peer_id.empty() || text.empty()) {
        return Result<void>::error(StatusCode::ERR_INVALID_ARG);
    }

    ChatMessage msg;
    msg.peer_id   = peer_id;
    msg.text      = text;
    msg.timestamp = Clock::now();
    msg.outgoing  = true;
    msg.delivered = false;  // Will be set when mesh confirms

    auto& conv = get_or_create(peer_id);
    conv.messages.push_back(msg);
    conv.last_activity = msg.timestamp;

    // Keep max 500 messages per conversation
    while (conv.messages.size() > 500) {
        conv.messages.pop_front();
    }

    log::info(TAG, "Sent to %s: \"%s\"", peer_id.c_str(), text.c_str());
    return Result<void>::success();
}

void SmsApp::receive(const std::string& peer_id, const std::string& text) {
    ChatMessage msg;
    msg.peer_id   = peer_id;
    msg.text      = text;
    msg.timestamp = Clock::now();
    msg.outgoing  = false;
    msg.delivered = true;

    auto& conv = get_or_create(peer_id);
    conv.messages.push_back(msg);
    conv.last_activity = msg.timestamp;
    conv.unread_count++;

    while (conv.messages.size() > 500) {
        conv.messages.pop_front();
    }

    log::info(TAG, "Received from %s: \"%s\"", peer_id.c_str(), text.c_str());

    // Notify callbacks
    for (auto& cb : m_callbacks) {
        if (cb) cb(msg);
    }
}

Conversation* SmsApp::get_conversation(const std::string& peer_id) {
    for (auto& conv : m_conversations) {
        if (conv.peer_id == peer_id) return &conv;
    }
    return nullptr;
}

void SmsApp::mark_read(const std::string& peer_id) {
    auto* conv = get_conversation(peer_id);
    if (conv) {
        conv->unread_count = 0;
    }
}

int SmsApp::total_unread() const {
    int count = 0;
    for (const auto& conv : m_conversations) {
        count += conv.unread_count;
    }
    return count;
}

void SmsApp::on_new_message(NewMessageFn fn) {
    m_callbacks.push_back(std::move(fn));
}

Conversation& SmsApp::get_or_create(const std::string& peer_id) {
    for (auto& conv : m_conversations) {
        if (conv.peer_id == peer_id) return conv;
    }
    // Create new conversation
    Conversation conv;
    conv.peer_id       = peer_id;
    conv.last_activity = Clock::now();
    conv.unread_count  = 0;
    m_conversations.push_back(std::move(conv));
    return m_conversations.back();
}

} // namespace vos
