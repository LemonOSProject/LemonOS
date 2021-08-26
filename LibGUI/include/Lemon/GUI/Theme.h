#pragma once

#include <Lemon/Core/ConfigManager.h>
#include <Lemon/Graphics/Colour.h>

#include <mutex>
#include <vector>

namespace Lemon::GUI {

enum {
    Colour_ActiveWindow,
    Colour_ActiveWindowText,
    Colour_InactiveWindow,
    Colour_InactiveWindowText,
    Colour_Background,         // Window Background, etc.
    Colour_ContainerBackground,  // Container Background, etc.
    Colour_Button,
    Colour_TextLight,          // Text used on light backgrounds
    Colour_TextDark,           // Text used on dark backgrounds
    Colour_Foreground,         // Used for highlighting, etc.
    Colour_ForegroundInactive,      // Used for highlighting, etc. when window or widget out of focus
    Colour_ForegroundAlternate,      // Alt. foreground colour
    Colour_Border, // Used for borders in buttons, etc.
    Colour_Count,
};

class Theme {
public:
    inline static Theme& Current() {
        static Theme theme;
        return theme;
    }

    void Update(const std::string& path);

    const Colour& ColourActiveWindow() const { return m_colours[Colour_ActiveWindow]; }
    const Colour& ColourActiveWindowText() const { return m_colours[Colour_ActiveWindowText]; }
    const Colour& ColourInactiveWindow() const { return m_colours[Colour_InactiveWindow]; }
    const Colour& ColourInactiveWindowText() const { return m_colours[Colour_InactiveWindowText]; }
    const Colour& ColourBackground() const { return m_colours[Colour_Background]; }

    const Colour& ColourContainerBackground() const { return m_colours[Colour_ContainerBackground]; }
    const Colour& ColourContentBackground() const { return m_colours[Colour_ContainerBackground]; }
    const Colour& ColourButton() const { return m_colours[Colour_Button]; }

    const Colour& ColourText() const { return m_colours[Colour_TextLight]; }
    const Colour& ColourTextLight() const { return m_colours[Colour_TextLight]; }
    const Colour& ColourTextDark() const { return m_colours[Colour_TextDark]; }

    const Colour& ColourForeground() const { return m_colours[Colour_Foreground]; }
    const Colour& ColourForegroundInactive() const { return m_colours[Colour_ForegroundInactive]; }
    const Colour& ColourForegroundAlternate() const { return m_colours[Colour_ForegroundAlternate]; }
    const Colour& ColourBorder() const { return m_colours[Colour_Border]; }

private:
    Theme() noexcept;

    std::vector<Colour> m_colours;

    //int m_titlebarHeight = 24;
    //int m_cornerRadius = 10;
    //int m_borderWidth = 1;

    std::mutex m_writeLock;
    ConfigManager m_config;
};

} // namespace Lemon::GUI