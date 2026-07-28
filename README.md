# fpp-VideoCapture

Video Capture plugin for [Falcon Player (FPP)](https://github.com/FalconChristmas/fpp). Captures video from local cameras (USB webcams, Raspberry Pi camera modules) or IP network cameras and displays the feed on pixel overlay models (LED matrices).

## Features

- **V4L2 Capture** — USB webcams and other Video4Linux2 devices on Linux
- **libcamera Capture** — Raspberry Pi camera modules (Pi Camera, HQ Camera, etc.)
- **IP Video Capture** — Network/IP cameras via RTSP/HTTP streams (e.g. `rtsp://...`, `http://...`)

## Installation

### Via FPP Plugin Manager

1. Navigate to **Content Setup → Plugins** in the FPP web UI.
2. Locate **Video Capture** in the list of available plugins and click **Install**.
3. Restart FPPD.

## Usage

1. Create a pixel overlay model in **Input/Output Setup → Pixel Overlay Models** matching your LED matrix layout. This is typically created automatically by xLights.
2. Trigger a **Video Capture** or **IP Video Capture** effect from a playlist.

### Via Playlist

1. Go to **Content Setup → Playlists** and create or edit a playlist.
2. Add a new entry, set **Type** to **Command**.
3. Set **Command** to `Pixel Overlay → Overlay Model Effect`.
4. Three fields appear:
   - **Models** — select your pixel overlay model
   - **Auto Enable/Disable** — keep `Enabled`
   - **Effect** — select `Video Capture` or `IP Video Capture`
5. When you select the effect, additional fields appear:

   **For Video Capture:**
   - **Camera** — enter the device path (e.g. `/dev/video0`) or `--Default--`

   **For IP Video Capture:**
   - **URL** — enter the stream URL (e.g. `rtsp://192.168.1.100:554/stream`)

6. Save the playlist and play it.

### Stopping an Effect

Add another Command entry with `Overlay Model Effect`, select `Stop Effects` as the Effect, and play it.

Or from **Status/Control → Effects** — running overlay effects appear under "Overlay Model Effects" with a Stop button.

## License

See [LICENSE](LICENSE).
