#include "3rdparty/parallel_hashmap/phmap.h"
#include "3rdparty/easylogging++.h"

namespace chainblocks {
  template<typename T>
  class ThreadShared {
    public:
      ThreadShared() {
        addRef();
      }

      ~ThreadShared() {
        decRef();
      }
      
      T& operator()() {
        return *_tp;
      }

    private:
      void addRef() {
        if(_refs == 0) {
          _tp = new T();
          DLOG(TRACE) << "Created a ThreadShared";
        }
        _refs++;
      }

      void decRef() {
        _refs--;
        if(_refs == 0) {
          delete _tp;
          _tp = nullptr;
          DLOG(TRACE) << "Deleted a ThreadShared";
        }
      }

      static inline thread_local uint64_t _refs = 0;
      static inline thread_local T *_tp = nullptr;
  };
};