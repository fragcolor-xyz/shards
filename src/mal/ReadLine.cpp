#include "ReadLine.h"
#include "String.h"
#include <replxx.hxx>

static replxx::Replxx rx{};

ReadLine::ReadLine(const String& historyFile)
: m_historyPath(historyFile)
{
    rx.history_load(m_historyPath.c_str());
}

ReadLine::~ReadLine()
{
}

bool ReadLine::get(const String& prompt, String& out)
{
    const char *line = rx.input(prompt);
    if (line == NULL) {
        return false;
    }
    rx.history_add(line); // Add input to in-memory history
    rx.history_save(m_historyPath.c_str());

    out = line;

    return true;
}
