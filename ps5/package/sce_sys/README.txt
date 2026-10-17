Home-screen art for the app:
  icon0.png  512x512 PNG - the project's own icon, drawn by ps5/tools/make_icon.py
  pic0.dds   optional: 3840x2160 background, BC7_UNORM DX10 DDS, no mipmaps
  pic1.dds   optional: the launch splash, same format (both or neither)
param.json is written by ps5/tools/ps5_package.sh.

The repository ships only the icon. The release zip on GitHub also carries a
background and the hub's theme; they are not in the repository. To use your
own background, make the DDS from any picture with:

    python3 ps5/tools/make_bc7_dds.py picture.png ps5/package/sce_sys/pic0.dds
    cp ps5/package/sce_sys/pic0.dds ps5/package/sce_sys/pic1.dds

(The background shows when the app launches; on the home screen, while the
app is only highlighted, it does not yet.)
