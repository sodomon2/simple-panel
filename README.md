# Simple Panel

<div align="center">
  <p>A lightweight, desktop panel for wlroots-based Wayland compositors</p>
  <p>
    <a href="https://github.com/sodomon2/simple-panel/blob/master/LICENSE">
      <img src="https://img.shields.io/github/license/sodomon2/simple-panel" alt="License">
    </a>
    <a href="https://github.com/sodomon2/simple-panel/releases">
      <img src="https://img.shields.io/github/release/sodomon2/simple-panel" alt="Releases">
    </a>
  </p>
</div>

![Screenshot of Simple Panel](https://github.com/sodomon2/project-screenshot/blob/master/simple-panel/screenshot.png?raw=true)

# Features
- Built with GTK4 and GIO for modern Wayland integration
- Support for **StatusNotifierItem** protocol
- Configuration via simple INI files
- Compatible with [Sway](https://github.com/swaywm/sway), [Hyprland](https://github.com/hyprwm/Hyprland/), [lawbc](https://github.com/labwc/labwc) and more

# Installation

```bash
# Clone the repository
git clone https://github.com/sodomon2/simple-panel.git
cd simple-panel

# Configure and build
meson setup build
ninja -C build

# Install (system-wide)
sudo ninja -C build install

# Run
simple-panel
```


## Contributing

Contributions are welcome! Feel free to submit issues and pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgements

- [GTK4](https://www.gtk.org/)
- [gtk4-layer-shell](https://github.com/wmww/gtk4-layer-shell)
- [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols)