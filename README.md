# simple-panel
A simple panel 

## Build and install

```
git clone https://github.com/sodomon2/simple-panel.git
meson setup build
ninja -C build
[sudo] ninja -C build install
```

## Dependencies

- [GTK4](https://www.gtk.org/)
- [GIO](https://developer.gnome.org/gio/stable/)
- [Gtk4LayerShell](https://github.com/wmww/gtk4-layer-shell)
- [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols)