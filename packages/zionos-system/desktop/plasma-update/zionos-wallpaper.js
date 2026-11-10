// Sets the ZionOS wallpaper once, when a user's Plasma desktop is first
// created. Later changes made by the user are never overridden.
var all = desktops();
for (var i = 0; i < all.length; i++) {
    var d = all[i];
    d.wallpaperPlugin = "org.kde.image";
    d.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
    d.writeConfig("Image", "file:///usr/share/wallpapers/ZionOS/");
}
