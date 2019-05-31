#pragma once


#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)


#ifndef WEBSOCKET_HANDLER_H_
#define WEBSOCKET_HANDLER_H_

#include "HttpServer.h"

class Conn: public WebSocketHandler {
public:
    Conn(WebSocket *webSocket);

	~Conn() override;

    void onClose() override;
    void onMessage(WebSocketInputStreambuf *pWebSocketInputStreambuf, WebSocket *pWebSocket) override;
    void onError(std::string error) override;

    void send(std::string data);
private:
    WebSocket *webSocket;
};

#endif

#endif

