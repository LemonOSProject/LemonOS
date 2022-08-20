#pragma once

#include <Lemon/Core/Icon.h>
#include <Lemon/Graphics/Surface.h>

#include <map>
#include <mutex>
#include <string>

namespace Lemon {
// Thread safe class for retrieve and caching icons
class IconManager final {
  public:
    enum IconSize {
        IconSize16x16,
        IconSize32x32,
        IconSize64x64,
    };

    static IconManager* Instance();

    /////////////////////////////
    /// \brief Get icon surface
    ///
    /// Attempts to find surface for icon of preferred size
    ///
    /// \param name Name of icon
    /// \param preferredSize Preferred icon size
    ///
    /// \return Immutable surface pointer
    /////////////////////////////
    const Surface* GetIcon(const std::string& name, IconSize preferredSize = IconSize32x32);

  private:
    IconManager();

    static IconManager* m_instance;
    static std::mutex m_mutex;

    Icon m_missingIcon; // Filler icon for when no sucessful icon could be found

    std::map<std::string, Icon> m_icons; // Icon cache
};
} // namespace Lemon
