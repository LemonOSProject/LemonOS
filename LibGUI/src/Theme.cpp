#include <Lemon/GUI/Theme.h>

#include <Lemon/GUI/WindowServer.h>

#define SET_COLOUR(c, val) 

namespace Lemon::GUI{

Theme::Theme() noexcept {
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.activeWindow", RGBAColour{0x22, 0x20, 0x22, 0xFF});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.activeWindowText", RGBAColour{0xEE, 0xEE, 0xEE, 0xFF});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.inactiveWindow", RGBAColour{0x22, 0x20, 0x22, 0xFF});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.inactiveWindowText", RGBAColour{0xEE, 0xEE, 0xEE, 0xFF});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.background", RGBAColour{});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.containerBackground", RGBAColour{});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.containerTextLight", RGBAColour{0xee, 0xee, 0xee, 0xff});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.containerTextDark", RGBAColour{0, 0, 0, 0xff});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.foreground", RGBAColour{});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.foregroundInactive", RGBAColour{});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.foregroundAlternate", RGBAColour{});
    m_config.AddSerializedConfigProperty<RGBAColour>("colours.border", RGBAColour{});

    m_colours.resize(Colour_Count);

    m_config.AddConfigProperty<long>("wm.windowCornerRadius", 10);
    m_config.AddConfigProperty<long>("wm.windowBorderThickness", 1);
    m_config.AddConfigProperty<long>("wm.windowTitleBarHeight", 24);
}

void Theme::Update(const std::string& path){
    std::unique_lock<std::mutex> preventWrite(m_writeLock);

    m_config.LoadJSONConfig(path);

    m_colours[Colour_ActiveWindow] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.activeWindow");
    m_colours[Colour_ActiveWindowText] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.activeWindowText");
    m_colours[Colour_InactiveWindow] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.inactiveWindow");
    m_colours[Colour_InactiveWindowText] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.inactiveWindowText");
    m_colours[Colour_Background] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.background");
    m_colours[Colour_ContainerBackground] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.containerBackground");
    m_colours[Colour_TextLight] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.containerTextLight");
    m_colours[Colour_TextDark] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.containerTextDark");
    m_colours[Colour_Foreground] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.foreground");
    m_colours[Colour_ForegroundInactive] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.foregroundInactive");
    m_colours[Colour_ForegroundAlternate] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.foregroundAlternate");
    m_colours[Colour_Border] = m_config.GetSerializedConfigProperty<RGBAColour>("colours.border");
}

}