#ifndef F927121D_987F_4BF6_BBC8_DDCE9839EFEE
#define F927121D_987F_4BF6_BBC8_DDCE9839EFEE

namespace shards {
  
struct CachedStreamBuf : std::streambuf {
  std::vector<char> data;

  void reset() { data.clear(); }

  std::streamsize xsputn(const char *s, std::streamsize n) override {
    data.insert(data.end(), &s[0], s + n);
    return n;
  }

  int overflow(int c) override {
    if (c != EOF) {
      data.push_back(static_cast<char>(c));
      return c; // Return the written character
    }
    return EOF;
  }
  void done() { data.push_back('\0'); }

  std::string_view str() {
    if (data.empty())
      return "";
    return std::string_view(data.data(), data.size() - 1);
  }
};

struct StringStreamBuf : std::streambuf {
  StringStreamBuf(const std::string_view &s) : data(s) {}

  std::string_view data;
  uint32_t index{0};
  bool done{false};

  std::streamsize xsgetn(char *s, std::streamsize n) override {
    if (n == 0)
      return 0;
    if (unlikely(done)) {
      return 0;
    } else if ((size_t(index) + n) > data.size()) {
      const auto len = data.size() - index;
      memcpy(s, &data[index], len);
      done = true;  // flag to indicate we are done
      index += len; // Update index correctly
      return len;
    } else {
      memcpy(s, &data[index], n);
      index += n;
      return n;
    }
  }

  int underflow() override {
    if (index >= data.size())
      return EOF;

    return data[index];
  }
};

struct VarStringStream {
  CachedStreamBuf cache;

  void write(const SHVar &var) {
    cache.reset();
    std::ostream stream(&cache);
    stream << var;
    cache.done();
  }

  void tryWriteHex(const SHVar &var) {
    cache.reset();
    std::ostream stream(&cache);
    if (var.valueType == SHType::Int) {
      stream << "0x" << std::hex << std::setw(2) << std::setfill('0') << var.payload.intValue;
    } else if (var.valueType == SHType::Bytes) {
      stream << "0x" << std::hex;
      for (uint32_t i = 0; i < var.payload.bytesSize; i++)
        stream << std::setw(2) << std::setfill('0') << (int)var.payload.bytesValue[i];
    } else if (var.valueType == SHType::Int16) {
      auto& s{ stream << std::hex << std::setw(2) << std::setfill('0')};
      for (int i = 0; i < 16; i++) {
        s << ((int)var.payload.int16Value[i] & 0xff);
      }
    } else if (var.valueType == SHType::String) {
      stream << "0x" << std::hex;
      auto len = var.payload.stringLen;
      if (len == 0 && var.payload.stringValue) {
        len = uint32_t(strlen(var.payload.stringValue));
      }
      for (uint32_t i = 0; i < len; i++)
        stream << std::setw(2) << std::setfill('0') << (int)var.payload.stringValue[i];
    } else {
      throw ActivationError("Cannot convert type to hex");
    }
    cache.done();
  }

  const std::string_view str() { return cache.str(); }
};
}

#endif /* F927121D_987F_4BF6_BBC8_DDCE9839EFEE */
