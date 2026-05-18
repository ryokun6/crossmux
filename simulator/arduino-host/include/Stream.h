#pragma once

#include <Print.h>

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;

  // Arduino's Stream API exposes a per-stream timeout used by helper read
  // operations (readBytes, timedRead). Host doesn't run those code paths, but
  // subclasses still call setTimeout() in their constructors.
  void setTimeout(unsigned long timeoutMs) { _timeout = timeoutMs; }
  unsigned long getTimeout() const { return _timeout; }

  // Read until terminator or timeout; default impl reads byte-by-byte.
  virtual String readStringUntil(char terminator);

 protected:
  unsigned long _timeout = 1000;
};
