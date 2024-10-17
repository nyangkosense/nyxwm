# nyxwm - Nyx Window Manager

nyxwm is a simple, lightweight X window manager forked from [sowm](https://github.com/dylanaraps/sowm) and incorporating features from [dwmblocks](https://github.com/torrinfail/dwmblocks). It aims to provide a minimalist yet functional desktop environment for Linux systems.

![rice](https://github.com/user-attachments/assets/a55adf8c-6a01-4a43-9849-a250923ecdad)

## Features

- Floating/fullscreen only, no tiling 
- Minimalist design philosophy
- Low resource usage
- Customizable statusbar with real-time updates
- Simple window management with keyboard shortcuts
- Support for multiple workspaces
- Integrated systray
- Autostart script support

## Dependencies

To build and run nyxwm, you'll need the following:

- Xlib header files
- libXft
- libX11
- make
- gcc

## Installation

1. Clone the repository:
   ```
   git clone https://github.com/nyangkosense/nyxwm.git
   cd nyxwm
   ```

2. Edit config.h / blocks.h

3. Build and install:
   ```
   make
   sudo make install
   ```

## Configuration

nyxwm is configured through the `config.h` file. You can modify keybindings, colors, and other settings here.

The status bar can be customized by editing the `blocks.h` file, where you can add or remove status elements.

### Default Keybindings

- `Mod4 + Enter`: Open terminal (st)
- `Mod4 + d`: Open dmenu
- `Mod4 + [1-9]`: Switch to workspace
- `Mod4 + Shift + [1-9]`: Move window to workspace
- `Mod4 + f`: Toggle fullscreen
- `Mod4 + q`: Close window

(Add more keybindings as per your configuration)

## Autostart

nyxwm looks for an autostart script at `~/.config/nyxwm/autostart.sh`. You can use this to launch programs or set up your environment when nyxwm starts.

## Contributing

Please feel free to submit pull requests or open issues for bugs and feature requests.

## Acknowledgments

nyxwm is forked from [sowm](https://github.com/dylanaraps/sowm) by Dylan Araps 
and incorporates ideas from [dwmblocks](https://github.com/torrinfail/dwmblocks) by torrinfail.

## License

nyxwm is distributed under the MIT License. See `LICENSE` file for more information.
