#include "pch.hpp"

#if defined(USE_WEB)

#include "websocket.hpp"

#include <map>
#include <mutex>

#include "JSON.h"


static const char *TAG = "[Web-ws]";

extern std::mutex conns_mutex;

extern std::vector<Conn *> conns;

/*
 * @brief Websocket handling
 */


Conn::Conn(WebSocket *webSocket): webSocket(webSocket) {

}

Conn::~Conn() {
    ESP_LOGI(TAG, "Conn deleted");
}

void Conn::onClose() {
    ESP_LOGD(TAG, "Websocket closed");
    std::lock_guard<std::mutex> guard(conns_mutex);

    conns.erase(std::remove(conns.begin(), conns.end(), this), conns.end());

    conns.shrink_to_fit();

    int len = conns.size();

    ESP_LOGD(TAG, "removed connection from list, now have %d connections", len);

    ESP_LOGI(TAG, "conn deleting self");

    delete this;
}

void Conn::onMessage(WebSocketInputStreambuf *pWebSocketInputStreambuf, WebSocket *pWebSocket) {
    ESP_LOGI(TAG, "Websocket message received");

    std::stringbuf strbuf;
    std::ostream os(&strbuf);
    os << pWebSocketInputStreambuf;

    std::string content(strbuf.str());

    JsonObject m = JSON::parseObject(content);

    auto command = m.getString("c");

    if (command == "volup") {
        volume_up();
    } else if (command == "voldown") {
        volume_down();
    } else {
        ESP_LOGD(TAG, "Unknown websocket command: %s", command.c_str());
    }

    JSON::deleteObject(m);
}

void Conn::onError(std::string error) {
    ESP_LOGE(TAG, "Websocket error: %s", error.c_str());
}

void Conn::send(std::string data) {
    if (this->webSocket != nullptr) {
        this->webSocket->send(data, 0x02);
    } else {
        ESP_LOGE(TAG, "Websocket conn has null inner connection pointer");
    }
}

#endif
