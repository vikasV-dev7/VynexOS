#pragma once
#include "vynexos/desktop/taskbar.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <vector>
#include <mutex>

namespace vynexos::desktop {

class BasicTaskbar final : public ITaskbar {
public:
    explicit BasicTaskbar(std::shared_ptr<IWidgetToolkit> toolkit);
    ~BasicTaskbar() override = default;
    
    bool synchronize(const std::vector<TaskbarEntry>& entries) override;
    void draw(std::shared_ptr<ISurface> surface) override;
    std::optional<uint32_t> get_clicked_window(int32_t x, int32_t y) const override;

private:
    std::shared_ptr<IWidgetToolkit> m_toolkit;
    mutable std::mutex m_mutex;
    std::vector<TaskbarEntry> m_entries;
    
    static constexpr int32_t kButtonWidth = 150;
    static constexpr int32_t kButtonHeight = 30;
    static constexpr int32_t kButtonMargin = 5;
    static constexpr int32_t kStartX = 100; // Leave room for "Vynex" launcher button
    static constexpr int32_t kStartY = 5;
};

} // namespace vynexos::desktop
