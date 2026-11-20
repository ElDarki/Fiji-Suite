# Flatpak build

Builds Fiji Suite as an extension of the Flathub OBS (`com.obsproject.Studio`), so it loads in Flatpak OBS.
Needs `flatpak` and `flatpak-builder` (on Windows: a privileged Fedora container in Docker). The runtime and
SDK below must match the ones `flatpak remote-info flathub com.obsproject.Studio` reports.

```
flatpak install flathub com.obsproject.Studio//stable org.freedesktop.Sdk//26.08
flatpak-builder --disable-rofiles-fuse --force-clean --repo=repo build build-aux/flatpak/com.obsproject.Studio.Plugin.FijiSuite.json
flatpak build-bundle --runtime repo fiji-suite-VERSION-linux-flatpak.flatpak com.obsproject.Studio.Plugin.FijiSuite stable
```

Users install it with `flatpak install --user fiji-suite-VERSION-linux-flatpak.flatpak`.
Update the release version and date in the `.metainfo.xml` with each release.
