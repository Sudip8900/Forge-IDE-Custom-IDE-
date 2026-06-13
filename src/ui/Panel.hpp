#pragma once
#include <atomic>

namespace forge {

class Panel {
public:
    virtual ~Panel() = default;
    
    // Primary render function called inside the ImGui context loop
    virtual void render() = 0;
    
    virtual const char* getName() const = 0;
    
    virtual bool isOpen() const {
        if (pendingOpen.exchange(false)) {
            open = true;
        }
        return open;
    }
    
    void setOpen(bool isOpen) {
        if (isOpen) {
            pendingOpen.store(true);
        } else {
            open = false;
            pendingOpen.store(false);
        }
    }

protected:
    mutable bool open = true;
    mutable std::atomic<bool> pendingOpen{false};
};

} // namespace forge
