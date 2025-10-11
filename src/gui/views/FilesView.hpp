#pragma once

#include <memory>
#include "../widgets/FileExplorerWidget.hpp"
#include "../../utils/SessionManager.hpp"

namespace SaperaCapturePro {

class FilesView {
public:
    FilesView(SessionManager* session_mgr);
    ~FilesView();

    void Render();

private:
    SessionManager* session_manager_;
    std::unique_ptr<FileExplorerWidget> file_explorer_;
};

} // namespace SaperaCapturePro
