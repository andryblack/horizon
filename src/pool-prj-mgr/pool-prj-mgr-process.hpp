#pragma once
#include "util/editor_process.hpp"
#include "util/uuid.hpp"

namespace horizon {
class PoolProjectManagerProcess {
public:
    enum class Type { IMP_SYMBOL, IMP_PADSTACK, IMP_PACKAGE, IMP_SCHEMATIC, IMP_BOARD, IMP_FRAME, UNIT, ENTITY, PART };
    PoolProjectManagerProcess(const UUID &uu, Type ty, const std::vector<std::string> &args,
                              const std::vector<std::string> &env, class Pool *pool,
                              class PoolParametric *pool_parametric, bool read_only, bool is_temp);
    UUID uuid;
    Type type;
    std::unique_ptr<EditorProcess> proc = nullptr;
    class EditorWindow *win = nullptr;
    typedef sigc::signal<void, int, bool> type_signal_exited;
    type_signal_exited signal_exited()
    {
        return s_signal_exited;
    }
    void reload();
    std::string get_filename();

private:
    type_signal_exited s_signal_exited;
    Glib::TimeVal mtime;
    std::string filename;
};
} // namespace horizon
