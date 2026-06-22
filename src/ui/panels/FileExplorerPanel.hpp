#pragma once

#include "../Panel.hpp"
#include "../../core/Workspace.hpp"

namespace forge {

class FileExplorerPanel : public Panel {
public:
    FileExplorerPanel() = default;
    ~FileExplorerPanel() override = default;

    void render() override;
    const char* getName() const override { return "PROJECT"; }

private:
    void renderNode(const WorkspaceFile& node);
};

} // namespace forge
