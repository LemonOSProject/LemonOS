#include <Lemon/GUI/Window.h>

#include <Lemon/GUI/Model.h>
#include <Lemon/GUI/Messagebox.h>
#include <Lemon/GUI/FileDialog.h>
#include <Lemon/GUI/Widgets.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Text.h>

#include "AudioContext.h"
#include "AudioTrack.h"

using namespace Lemon;
using namespace Lemon::GUI;

#define BUTTON_PADDING 5

class PlayButton : public Button {
public:
    PlayButton(AudioContext* ctx)
        : Button("", {0, 0, 0, 0}), m_ctx(ctx) {
        // Get the length of the labels in pixels
        // using the font
        playLabelTextLength = Graphics::GetTextLength(playLabel.c_str());
        pauseLabelTextLength = Graphics::GetTextLength(playLabel.c_str());
    }

    void OnMouseDown(vector2i_t mousePos) override {
        // Call the parent function to handle 
        // visual feedback for button presses
        Button::OnMouseDown(mousePos);
    }

    void Paint(surface_t* surface) override {
        if(m_ctx->IsAudioPlaying()) {
            label = pauseLabel;
            labelLength = pauseLabelTextLength;
        } else {
            label = playLabel;
            labelLength = playLabelTextLength;
        }

        Button::Paint(surface);
    }
    
private:
    AudioContext* m_ctx;

    std::string playLabel = "Play";
    int playLabelTextLength = 0;
    std::string pauseLabel = "Pause";
    int pauseLabelTextLength = 0;
};

class StopButton : public Button {
public:
    StopButton(AudioContext* ctx)
        : Button("Stop", {0, 0, 0, 0}), m_ctx(ctx) {

    }

    void OnMouseDown(vector2i_t mousePos) override {
        // Call the parent function to handle 
        // visual feedback for button presses
        Button::OnMouseDown(mousePos);

        m_ctx->PlaybackStop();
    }

    void Paint(surface_t* surface) override {
        Button::Paint(surface);
    }
    
private:
    AudioContext* m_ctx;
};

class PlayerWidget : public Container {
public:
    PlayerWidget(AudioContext* ctx) : Container({0, 0, 0, 200}), m_ctx(ctx) {
        m_playerControls = new LayoutContainer({0, 0, 0, 32 + BUTTON_PADDING * 2}, {80, 32});

        m_play = new PlayButton(m_ctx);
        m_previousTrack = new Button("Prev", {0, 0, 0, 0});
        m_stop = new StopButton(m_ctx);
        m_nextTrack = new Button("Next", {0, 0, 0, 0});

        // The player widget will fill the parent container
        SetLayout(LayoutSize::Stretch, LayoutSize::Fixed);
        m_playerControls->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WidgetAlignment::WAlignLeft,
                                    WidgetAlignment::WAlignBottom);
        m_playerControls->xFill = true;
        m_playerControls->xPadding = BUTTON_PADDING;

        AddWidget(m_playerControls);
        m_playerControls->AddWidget(m_play);
        m_playerControls->AddWidget(m_previousTrack);
        m_playerControls->AddWidget(m_stop);
        m_playerControls->AddWidget(m_nextTrack);

        (void)m_ctx;
    }

    void Paint(Surface* surface) override { Container::Paint(surface); }

    void UpdateFixedBounds() override {
        // Make sure UpdateFixedBounds has not been called before the constructor
        // Really checking one widget would be enough as if one has not been allocated
        // the others most likely also havent
        assert(m_previousTrack && m_play && m_nextTrack);

        Container::UpdateFixedBounds();
    }

    ~PlayerWidget() {
        delete m_play;
        delete m_previousTrack;
        delete m_stop;
        delete m_nextTrack;
    }

    inline AudioContext* Context() { return m_ctx; }

private:
    AudioContext* m_ctx;

    Graphics::TextObject m_duration;

    LayoutContainer* m_playerControls;
    PlayButton* m_play = nullptr;
    Button* m_previousTrack = nullptr;
    StopButton* m_stop = nullptr;
    Button* m_nextTrack = nullptr;
};

class InfoWidget : public Container {};

void OnOpenTrack(Button*);
void OnSubmitTrack(int, ListView*);

class TrackSelection : public Container, public DataModel {
public:
    TrackSelection(AudioContext* ctx) : Container({0, 200, 0, 40}), m_ctx(ctx) {
        // Fill the parent container
        SetLayout(LayoutSize::Stretch, LayoutSize::Stretch);

        m_listView = new ListView({0, 0, 0, 37});

        AddWidget(m_listView);

        m_listView->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch);
        m_listView->SetModel(this);

        m_openTrack = new Button("Open File...", {5, 5, 120, 32});
        m_openTrack->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WidgetAlignment::WAlignLeft,
                               WidgetAlignment::WAlignBottom);

        AddWidget(m_openTrack);

        m_openTrack->OnPress = OnOpenTrack;
        m_listView->OnSubmit = OnSubmitTrack;
    }

    int LoadTrack(std::string filepath) {
        TrackInfo track;
        if(int r = m_ctx->LoadTrack(filepath, track); r) {
            return r;
        }

        m_tracks.push_back(track);
        return 0;
    }

    int PlayTrack(int index) {
        if(!m_tracks.size()) {
            return 0;
        }

        return m_ctx->PlayTrack(m_tracks.at(index).filepath);
    }

    int ColumnCount() const override { return s_numFields; }
    int RowCount() const override { return (int)m_tracks.size(); }

    const char* ColumnName(int column) const override {
        assert(column < s_numFields);
        return s_fields[column];
    }

    Variant GetData(int row, int column) override {
        assert(row < (int)m_tracks.size());

        TrackInfo& track = m_tracks.at(row);
        if (column == 0) {
            return track.filepath;
        } else if (column == 1) {
            return track.metadata.artist + " - " + track.metadata.title;
        } else if (column == 2) {
            return track.durationString;
        }

        return 0;
    }

    int SizeHint(int column) override {
        assert(column < s_numFields);
        return s_fieldSizes[column];
    }

private:
    static const int s_numFields = 3;
    static constexpr const char* s_fields[s_numFields]{"File", "Track", "Duration"};
    static constexpr int s_fieldSizes[s_numFields]{200, 200, 60};

    AudioContext* m_ctx;

    ListView* m_listView;
    Button* m_openTrack;

    std::vector<TrackInfo> m_tracks;
};

void OnOpenTrack(Button* button) {
    TrackSelection* tracks = (TrackSelection*)button->GetParent();
    assert(tracks);

    char* filepath = FileDialog(".", FILE_DIALOG_DIRECTORIES);
    if(!filepath) {
        return;
    }

    if(tracks->LoadTrack(filepath)) {
        std::string message = std::string("Failed to load '") + filepath + '\'';
        DisplayMessageBox("Error load file", message.c_str());
    }
    delete filepath;
}

void OnSubmitTrack(int row, ListView* lv) {
    TrackSelection* tracks = (TrackSelection*)lv->GetParent();
    assert(tracks);

    if(tracks->PlayTrack(row)) {
        return;
    }

    return;
}

int main(int argc, char** argv) {
    AudioContext* audio = new AudioContext();

    Window* window = new Window("Audio Player", {480, 640}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);
    PlayerWidget* player = new PlayerWidget(audio);
    TrackSelection* tracks = new TrackSelection(audio);

    window->AddWidget(player);
    window->AddWidget(tracks);

    while (!window->closed) {
        Lemon::WindowServer::Instance()->Poll();

        window->GUIPollEvents();
        window->Paint();

        Lemon::WindowServer::Instance()->Wait();
    }

    delete window;
    delete player;
    delete tracks;
    delete audio;
    return 0;
}
