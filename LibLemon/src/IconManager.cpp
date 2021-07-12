#include <Lemon/Core/IconManager.h>

#include <Lemon/Graphics/Graphics.h>

namespace Lemon {
IconManager* IconManager::m_instance = nullptr;
std::mutex IconManager::m_mutex;

IconManager::IconManager() {
    m_missingIcon = {
        .icon16 =
            {
                .width = 16,
                .height = 16,
                .depth = 32,
                .buffer = new uint8_t[16 * 16 * 4],
            },
        .icon32 =
            {
                .width = 32,
                .height = 32,
                .depth = 32,
                .buffer = new uint8_t[32 * 32 * 4],
            },
        .icon64 =
            {
                .width = 64,
                .height = 64,
                .depth = 32,
                .buffer = new uint8_t[64 * 64 * 4],
            },
    };
}

IconManager* IconManager::Instance() {
    std::scoped_lock lock(m_mutex);

    if (!m_instance) {
        m_instance = new IconManager();
    }

    return m_instance;
}

const Surface* IconManager::GetIcon(const std::string& name, IconSize preferredSize) {
    Icon& icon = m_icons[name];

    if (preferredSize == IconSize16x16) {
        if (icon.icon16.buffer) {
            return &icon.icon16;
        }

        // Try all sizes if unsuccessful, scale if necessary
        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/16/") + name + ".png").c_str(), 0,
                                        0, 16, 16, &icon.icon16, false)) {
            return &icon.icon16;
        }

        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/32/") + name + ".png").c_str(), 0,
                                        0, 16, 16, &icon.icon16, false)) {
            return &icon.icon16;
        }

        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/64/") + name + ".png").c_str(), 0,
                                        0, 16, 16, &icon.icon16, false)) {
            return &icon.icon16;
        }

        return &m_missingIcon.icon16; // Unsuccessful
    }

    if (preferredSize == IconSize32x32) {
        if (icon.icon32.buffer) {
            return &icon.icon32;
        }

        // Try all sizes if unsuccessful, scale if necessary
        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/32/") + name + ".png").c_str(), 0,
                                        0, 32, 32, &icon.icon32, false)) {
            return &icon.icon32;
        }

        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/64/") + name + ".png").c_str(), 0,
                                        0, 32, 32, &icon.icon32, false)) {
            return &icon.icon32;
        }

        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/16/") + name + ".png").c_str(), 0,
                                        0, 32, 32, &icon.icon32, false)) {
            return &icon.icon32;
        }

        return &m_missingIcon.icon32; // Unsuccessful
    }

    if (preferredSize == IconSize64x64) {
        if (icon.icon64.buffer) {
            return &icon.icon64;
        }

        // Try all sizes if unsuccessful, scale if necessary
        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/64/") + name + ".png").c_str(), 0,
                                        0, 64, 64, &icon.icon64, false)) {
            return &icon.icon64;
        }

        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/32/") + name + ".png").c_str(), 0,
                                        0, 64, 64, &icon.icon64, false)) {
            return &icon.icon64;
        }

        if (!Lemon::Graphics::LoadImage((std::string("/system/lemon/resources/icons/16/") + name + ".png").c_str(), 0,
                                        0, 64, 64, &icon.icon64, false)) {
            return &icon.icon64;
        }

        return &m_missingIcon.icon64; // Unsuccessful
    }

    assert(!"Invalid icon preferred size!");
    return nullptr;
}
} // namespace Lemon
