#ifndef INCLUDE_ENVIRONMENT_H
#define INCLUDE_ENVIRONMENT_H

#include "MAL.h"

#include <map>

class malEnv : public RefCounted {
public:
    malEnv(malEnvPtr outer = NULL);
    malEnv(malEnvPtr outer,
           const StringVec& bindings,
           malValueIter argsBegin,
           malValueIter argsEnd);

    ~malEnv();

    malValuePtr get(const String& symbol);
    malEnvPtr   find(const String& symbol);
    malValuePtr set(const String& symbol, malValuePtr value);
    malEnvPtr   getRoot();

    const String &currentPath() const { return m_currentPath; }
    void currentPath(String path) {
      m_currentPath = path;
    }

private:
    String m_currentPath;
    typedef std::map<String, malValuePtr> Map;
    Map m_map;
    malEnvPtr m_outer;
};

#endif // INCLUDE_ENVIRONMENT_H
