#pragma once

#include <ESPAsyncWebServer.h>

class WebSocketPrint : public Print {
public:
  explicit WebSocketPrint(AsyncWebSocket* ws = nullptr) : _ws(ws) {}

  size_t write(uint8_t c) override {
    if (_ws) {
      _tx += (char)c;
      if (c == '\n') flush();
    }
    return 1;
  }
  size_t write(const uint8_t* b, size_t n) override {
    if (_ws) {
      for (size_t i = 0; i < n; ++i) write(b[i]);
    }
    return n;
  }
  void flush() override {
    if (_ws && _tx.length()) { _ws->textAll(_tx); _tx = ""; }
  }

private:
  AsyncWebSocket* _ws;
  String _tx;
};

class MultiPrint : public Print {
  public:
    MultiPrint() = default;
    MultiPrint(Print& p) { addPrint(&p); }
    MultiPrint(Print* p) { addPrint(p); }

    void addPrint(Print* p) {
      _prints.push_back(p);
    }

    size_t write(uint8_t c) override {
      for (auto p : _prints) p->write(c);
      return 1;
    }

    size_t write(const uint8_t* b, size_t n) override {
      for (auto p : _prints) p->write(b, n);
      return n;
    }

    void flush() override {
      for (auto p : _prints) p->flush();
    }

  private:
    std::vector<Print*> _prints;
};
