#include <lemon/gui/Window.h>

#include <lemon/gui/FileDialog.h>
#include <lemon/gui/Messagebox.h>
#include <lemon/gui/Model.h>
#include <lemon/gui/Widgets.h>
#include <lemon/graphics/Graphics.h>
#include <lemon/graphics/Text.h>

#include <lemon/core/Format.h>
#include <lemon/core/JSON.h>

#include <dirent.h>
#include <sys/stat.h>

#include "AudioContext.h"
#include "AudioTrack.h"

#include <unordered_map>

using namespace Lemon;
using namespace Lemon::GUI;

// Padding inbetween container widgets
#define CONTAINER_PADDING 5
// Height of the playback progress bar
#define PROGRESSBAR_HEIGHT 20

void OnOpenTrack(class TrackSelection*);
void OnNextTrack(Button*);
void OnPrevTrack(Button*);
void OnSubmitTrack(int, ListView*);

class TrackSelection : public Container, public DataModel {
public:
    TrackSelection(AudioContext* ctx) : Container({0, 200, 0, 0}), m_ctx(ctx) {
        // Fill the parent container
        SetLayout(LayoutSize::Stretch, LayoutSize::Stretch);

        m_listView = new ListView({0, 0, 0, 37});

        AddWidget(m_listView);

        m_listView->SetLayout(LayoutSize::Stretch, LayoutSize::Stretch);
        m_listView->SetModel(this);

        m_openTrack = new Button("Open File or Folder...", {CONTAINER_PADDING, CONTAINER_PADDING, 160, 32});
        m_openTrack->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WidgetAlignment::WAlignLeft,
                               WidgetAlignment::WAlignBottom);

        AddWidget(m_openTrack);

        m_openTrack->e.onPress.Set(OnOpenTrack, this);
        m_listView->OnSubmit = OnSubmitTrack;

        // Initialize the random generator,
        // it is only used for shuffle so just use rand()
        // despite how its not that random
        srand(time(NULL));
    }

    void ToggleShuffle() { m_trackQueueShuffle = !m_trackQueueShuffle; }

    bool ShuffleIsOn() const { return m_trackQueueShuffle; }

    int LoadTrack(const std::string& filepath) {
        // Attempts to load the metadata of the specified
        // audio file.
        // If it was successfully read, add to the tracklist

        TrackInfo track;
        if (int r = m_ctx->LoadTrack(filepath, &track); r) {
            return r;
        }

        m_tracks[filepath] = std::move(track);
        m_trackList.push_back(&m_tracks.at(filepath));

        m_listView->UpdateData();
        return 0;
    }

    int LoadDirectory(const std::string& filepath) {
        DIR* dir;
        if (dir = opendir(filepath.c_str()); !dir) {
            return 1;
        }

        // Read each entry in the directory
        struct dirent* ent;
        while ((ent = readdir(dir))) {
            if (ent->d_name[0] == '.') {
                // Ignore hidden or 'dot' files including . and ..
                continue;
            }

            // Append the entry name to the path of the directory to get the
            // new file to load
            std::string newPath = filepath + "/" + ent->d_name;
            LoadFilepath(newPath);
        }

        return 0;
    }

    int LoadFilepath(std::string path) {
        // Attempts to load a file,
        // detects if it is a directory and if so recursively opens
        // each file in the directory.

        struct stat s;
        if (stat(path.c_str(), &s)) {
            DisplayMessageBox(path.c_str(), fmt::format("{} attempting to read {}", strerror(errno), path).c_str());
        }

        if (S_ISDIR(s.st_mode)) {
            LoadDirectory(path);
        } else if (int e = LoadTrack(path); e) {
            DisplayMessageBox("Error load file", fmt::format("Failed to load {}", path).c_str());
            return e;
        }

        return 0;
    }

    int LoadPlaylist(std::string path) { return 0; }

    int SavePlaylist(std::string path) { return 0; }

    int PlayTrack(int index) {
        // If there are no tracks there is nothing to play
        if (!m_trackList.size()) {
            return 0;
        }

        m_trackIndex = index;
        return m_ctx->PlayTrack(m_trackList.at(index));
    }

    int RemoveTrack(int index) {
        if (!m_trackList.size()) {
            return 0;
        }

        TrackInfo* track = m_trackList.at(index);
        // Check we actually removed a track
        assert(m_tracks.erase(track->filepath) > 0);
        m_trackList.erase(m_trackList.begin() + index);

        m_listView->UpdateData();
        
        ResetQueue();

        return 0;
    }

    // ColmunCount, RowCount and ColumnName are called by the ListView GUI widget
    // to get the amount of columns, amount of rows (each track takes up a row)
    // and the name of each column (e.g. "File", "Track" or Duration)
    int ColumnCount() const override { return s_numFields; }
    int RowCount() const override { return (int)m_trackList.size(); }

    const char* ColumnName(int column) const override {
        assert(column < s_numFields);
        return s_fields[column];
    }

    // GetData is called by the ListView widget.
    // GetData will return the data to be displayed at the specified
    // row and column in the list.
    Variant GetData(int row, int column) override {
        assert(row < (int)m_trackList.size());

        TrackInfo& track = *m_trackList.at(row);
        if (column == 0) {
            return track.filename;
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

    void NextTrack() {
        m_ctx->PlaybackStop();

        if (m_trackList.size() > 0) {
            if (m_trackQueueShuffle) {
                // If there are too many tracks in the queue
                // remove the entry at the front
                if (m_trackQueuePrevious.size() >= m_trackQueueMax) {
                    m_trackQueuePrevious.pop_front();
                }

                // If m_trackIndex is valid, add it to the previous queue
                // When the 'Prev' button is pressed and PrevTrack() is called,
                // the previous track can be popped from the queue
                if (m_trackIndex > 0) {
                    m_trackQueuePrevious.push_back(m_trackIndex);
                }

                int newTrack = m_trackIndex;
                // Check if there is more than 1 track
                if (m_trackList.size() > 1) {
                    // Keep generating a new track index
                    // until we get a different track
                    while (newTrack == m_trackIndex)
                        newTrack = rand() % m_trackList.size();
                } else {
                    newTrack = 0;
                }

                PlayTrack(newTrack);
            } else {
                // Go to the next track in queue sequentially
                m_trackIndex++;
                if (m_trackIndex >= (int)m_trackList.size()) {
                    // If we are at the end of the track list, just stop playback for now.
                    // If NextTrack is called again, trackIndex will become 0 and the first song will be played.
                    m_trackIndex = -1;
                } else {
                    PlayTrack(m_trackIndex);
                }
            }
        }
    }

    void PrevTrack() {
        m_ctx->PlaybackStop();

        if (m_trackList.size() > 0) {
            if (m_trackQueueShuffle) {
                if (m_trackQueuePrevious.size()) {
                    PlayTrack(m_trackQueuePrevious.back());
                    m_trackQueuePrevious.pop_back();
                } else {
                    m_trackIndex = -1;
                }
            } else {
                m_trackIndex--;
                if (m_trackIndex < 0) {
                    m_trackIndex = m_trackList.size() - 1;
                }

                PlayTrack(m_trackIndex);
            }
        }
    }

private:
    void ResetQueue() { m_trackQueuePrevious.clear(); }

    static const int s_numFields = 3;
    static constexpr const char* s_fields[s_numFields]{"File", "Track", "Duration"};
    static constexpr int s_fieldSizes[s_numFields]{200, 200, 60};

    AudioContext* m_ctx;

    ListView* m_listView;
    Button* m_openTrack;

    std::unordered_map<std::string, TrackInfo> m_tracks;
    std::vector<TrackInfo*> m_trackList;

    // Should shuffle tracks
    bool m_trackQueueShuffle = false;
    // Maximum entries in track queue
    const unsigned m_trackQueueMax = 50;
    // Index in m_trackList
    int m_trackIndex = -1;
    // Previously played tracks
    std::list<int> m_trackQueuePrevious;
};

class ShuffleButton : public Button {
public:
    ShuffleButton(TrackSelection* tracks) : Button("Shuffle", {0, 0, 100, 24}), m_tracks(tracks) {}

    void OnMouseDown(vector2i_t mousePos) override { m_tracks->ToggleShuffle(); }

    void Paint(surface_t* surface) override {
        // Draw as 'pressed' if shuffle is enabled
        pressed = m_tracks->ShuffleIsOn();

        Button::Paint(surface);
    }

private:
    TrackSelection* m_tracks;
};

class PlayButton : public Button {
public:
    PlayButton(AudioContext* ctx) : Button("", {0, 0, 0, 0}), m_ctx(ctx) {
        // Get the length of the labels in pixels
        // using the font
        playLabelTextLength = Graphics::GetTextLength(playLabel.c_str());
        pauseLabelTextLength = Graphics::GetTextLength(playLabel.c_str());
    }

    void OnMouseDown(vector2i_t mousePos) override {
        // Call the parent function to handle
        // visual feedback for button presses
        Button::OnMouseDown(mousePos);

        if(m_ctx->IsAudioPlaying()) {
            m_ctx->PlaybackPause();
        } else {
            m_ctx->PlaybackStart();
        }
    }

    void Paint(surface_t* surface) override {
        if (m_ctx->IsAudioPlaying()) {
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
    StopButton(AudioContext* ctx) : Button("Stop", {0, 0, 0, 0}), m_ctx(ctx) {}

    void OnMouseDown(vector2i_t mousePos) override {
        // Call the parent function to handle
        // visual feedback for button presses
        Button::OnMouseDown(mousePos);

        m_ctx->PlaybackStop();
    }

    void Paint(surface_t* surface) override { Button::Paint(surface); }

private:
    AudioContext* m_ctx;
};

void OnOpenTrack(TrackSelection* tracks) {
    assert(tracks);

    char* filepath = FileDialog(".", FILE_DIALOG_DIRECTORIES);
    if (!filepath) {
        return;
    }

    tracks->LoadFilepath(filepath);
    delete filepath;
}

void OnSubmitTrack(int row, ListView* lv) {
    TrackSelection* tracks = (TrackSelection*)lv->GetParent();
    assert(tracks);

    if (tracks->PlayTrack(row)) {
        return;
    }

    return;
}

void OnNextTrack(TrackSelection* t) { t->NextTrack(); }

void OnPrevTrack(TrackSelection* t) { t->PrevTrack(); }

class PlayerWidget : public Container {
public:
    PlayerWidget(AudioContext* ctx, TrackSelection* ts) : Container({0, 0, 0, 200}), m_ctx(ctx), m_tracks(ts) {
        m_playerControls = new LayoutContainer({0, 0, 0, 32 + CONTAINER_PADDING * 2}, {80, 32});

        m_play = new PlayButton(m_ctx);
        m_previousTrack = new Button("Prev", {0, 0, 0, 0});
        m_stop = new StopButton(m_ctx);
        m_nextTrack = new Button("Next", {0, 0, 0, 0});
        m_shuffle = new ShuffleButton(m_tracks);

        // The player widget will fill the parent container
        // Get the buttons to fill the width of the widget
        SetLayout(LayoutSize::Stretch, LayoutSize::Fixed);
        m_playerControls->SetLayout(LayoutSize::Stretch, LayoutSize::Fixed, WidgetAlignment::WAlignLeft,
                                    WidgetAlignment::WAlignBottom);
        m_playerControls->xFill = true;
        m_playerControls->xPadding = CONTAINER_PADDING;

        // Draw the shuffle button 5 px to the right, above the progress bar
        // The position will be set in UpdateFixedBounds()
        m_shuffle->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WidgetAlignment::WAlignRight,
                             WidgetAlignment::WAlignTop);
        AddWidget(m_shuffle);

        AddWidget(m_playerControls);
        m_playerControls->AddWidget(m_play);
        m_playerControls->AddWidget(m_previousTrack);
        m_playerControls->AddWidget(m_stop);
        m_playerControls->AddWidget(m_nextTrack);

        m_nextTrack->e.onPress.Set(OnNextTrack, m_tracks);
        m_previousTrack->e.onPress.Set(OnPrevTrack, m_tracks);

        m_duration.SetColour(Theme::Current().ColourText());
        m_filepathText.SetColour(Theme::Current().ColourText());
        m_songText.SetColour(Theme::Current().ColourText());
        m_albumText.SetColour(Theme::Current().ColourText());
    }

    void Paint(Surface* surface) override {
        Container::Paint(surface);

        int totalDuration = 0;
        // Check if there is a track loaded
        if (const TrackInfo* info = m_ctx->CurrentTrack(); info) {
            int songProgress = m_ctx->PlaybackProgress();
            totalDuration = info->duration;

            // Update the filepath text to display the current file name
            m_filepathText.SetText(info->filename);
            // Display the current song as Song: <title> - <artist>
            m_songText.SetText(fmt::format("Song: {} - {}", info->metadata.title, info->metadata.album));
            m_albumText.SetText(fmt::format("Album: {}", info->metadata.album));

            // Only draw if there is a current track
            m_filepathText.BlitTo(surface);
            m_songText.BlitTo(surface);
            m_albumText.BlitTo(surface);

            // Use a format string to make the duraiton text
            // xx:xx/yy:yy where xx:xx is playback progress, yy:yy is total duration
            auto duration = fmt::format("{:02}:{:02}/{:02}:{:02}", songProgress / 60, songProgress % 60, totalDuration / 60,
                                        totalDuration % 60);
            m_duration.SetText(duration);
        } else {
            m_duration.SetText("00:00/00:00");
        }

        m_duration.BlitTo(surface);

        int progressBarY = ProgressbarRect().y;
        int progressBarWidth = fixedBounds.width - 10;
        Lemon::Graphics::DrawRoundedRect(ProgressbarRect(), Theme::Current().ColourContainerBackground(), 5, 5, 5, 5,
                                         surface);
        if (totalDuration > 0) {
            // Using the foreground colour specified in the system theme
            // draw a bar to indicate what has already been played
            float progress = std::clamp(m_ctx->PlaybackProgress() / totalDuration, 0.f, 1.f);
            Lemon::Graphics::DrawRoundedRect(
                Rect{fixedBounds.x + 5, progressBarY, static_cast<int>(progressBarWidth * progress), 10},
                Theme::Current().ColourForeground(), 5, 0, 0, 5, surface);

            // Effectively draw a circle by drawing a rounded rect with corner radii half the width of the rectangle
            // Draw to indicate where the progress is
            Lemon::Graphics::DrawRoundedRect(
                Rect{fixedBounds.x + 5 + static_cast<int>(progressBarWidth * progress) - 6, progressBarY - 1, 12, 12},
                Theme::Current().ColourText(), 6, 6, 6, 6, surface);
        }
    }

    void OnMouseDown(Vector2i pos) override {
        Rect pRect = ProgressbarRect();
        // Check we clicked on the progressbar,
        // if so, start seeking
        if (m_ctx->HasLoadedAudio() && Lemon::Graphics::PointInRect(pRect, pos)) {
            m_isSeeking = true;
        }

        Container::OnMouseDown(pos);
    }

    void OnMouseUp(Vector2i pos) override {
        Container::OnMouseUp(pos);

        // The user will no longer be pressing on the progress bar,
        // stop seeking to mouse position
        m_isSeeking = false;
    }

    void OnMouseMove(Vector2i pos) override {
        // If we are currently seeking (user held mouse down on progress bar)
        // and there is audio playing,
        // seek accordingly
        if (m_isSeeking && m_ctx->HasLoadedAudio()) {
            // Get the mouse position on the progress bar
            // as a percentage of the total width.
            // Clamp between 0 and 1 incase the mouse is left or right of the progress abr
            Rect pRect = ProgressbarRect();
            float percentage = std::clamp((float)(pos.x - pRect.x) / pRect.width, 0.f, 1.f);

            // Get seek timestamp by multiplying the total duration
            // by our percentage
            m_ctx->PlaybackSeek(percentage * m_ctx->CurrentTrack()->duration);
        }

        Container::OnMouseMove(pos);
    }

    void UpdateFixedBounds() override {
        // Make sure UpdateFixedBounds has not been called before the constructor
        // Really checking one widget would be enough as if one has not been allocated
        // the others most likely also havent
        assert(m_previousTrack && m_play && m_nextTrack && m_shuffle);

        Container::UpdateFixedBounds();
        m_duration.SetPos(fixedBounds.pos + Vector2i{5, m_play->GetFixedBounds().y - 5 - 15 - m_duration.Size().y});
        // Draw shuffle button in line with duration
        m_shuffle->SetBounds({CONTAINER_PADDING, m_duration.Pos().y, 120, 24});

        int textHeight = Graphics::DefaultFont()->lineHeight;

        m_filepathText.SetPos(fixedBounds.pos + Vector2i{5, 5});
        m_songText.SetPos(fixedBounds.pos + Vector2i{5, 5 + textHeight});
        m_albumText.SetPos(fixedBounds.pos + Vector2i{5, 5 + textHeight * 2});
    }

    ~PlayerWidget() {
        delete m_play;
        delete m_previousTrack;
        delete m_stop;
        delete m_nextTrack;
        delete m_shuffle;
    }

    inline AudioContext* Context() { return m_ctx; }

private:
    inline Rect ProgressbarRect() const {
        return {fixedBounds.x + CONTAINER_PADDING, fixedBounds.y + m_play->GetFixedBounds().y - CONTAINER_PADDING - 10,
                fixedBounds.width - 10, 10};
    }

    AudioContext* m_ctx;
    TrackSelection* m_tracks;

    Graphics::TextObject m_duration;
    Graphics::TextObject m_filepathText;
    Graphics::TextObject m_songText;
    Graphics::TextObject m_albumText;

    LayoutContainer* m_playerControls;
    PlayButton* m_play = nullptr;
    Button* m_previousTrack = nullptr;
    StopButton* m_stop = nullptr;
    Button* m_nextTrack = nullptr;
    ShuffleButton* m_shuffle = nullptr;

    // If the the mouse was pressed in the progress bar,
    // move the seek circle thingy to the mouse x coords
    bool m_isSeeking;
};

int main(int argc, char** argv) {
    AudioContext* audio = new AudioContext();

    Window* window = new Window("Audio Player", {480, 640}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);
    TrackSelection* tracks = new TrackSelection(audio);
    PlayerWidget* player = new PlayerWidget(audio, tracks);

    window->AddWidget(player);
    window->AddWidget(tracks);

    // If arguments are given to AudioPlayer,
    // (such as when opening a file in FileManager or running in Terminal)
    // then treat each argument as a file to be opened
    if (argc > 1) {
        int argi = 1;
        while (argc > 1) {
            tracks->LoadTrack(argv[argi++]);
            argc--;
        }

        tracks->PlayTrack(0);
    }

    while (!window->closed) {
        if (audio->ShouldPlayNextTrack()) {
            tracks->NextTrack();
        }

        Lemon::WindowServer::Instance()->Poll();

        window->GUIPollEvents();
        window->Paint();

        // Repaint every 200ms regardless of GUI events
        // to update the playback info
        Lemon::WindowServer::Instance()->Wait(200000);
    }

    delete window;
    delete player;
    delete tracks;
    delete audio;
    return 0;
}
