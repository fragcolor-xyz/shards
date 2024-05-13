#ifndef DCBD4FEB_B1F1_4618_8DCB_5603B3D3DC67
#define DCBD4FEB_B1F1_4618_8DCB_5603B3D3DC67

#include <string_view>

namespace shards::input {

struct Clipboard {
private:
  void *data;

public:
  Clipboard(void *impl);
  ~Clipboard();
  Clipboard(Clipboard &&);
  Clipboard &operator=(Clipboard &&);

  operator std::string_view() const;
};

const char *getClipboard();
Clipboard setClipboard();
} // namespace shards::input

#endif /* DCBD4FEB_B1F1_4618_8DCB_5603B3D3DC67 */
