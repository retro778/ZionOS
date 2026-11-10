/* ZionOS installer slideshow. SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick 2.0;
import calamares.slideshow 1.0;

Presentation
{
    id: presentation

    Slide {
        anchors.fill: parent
        Image {
            id: background
            source: "slide1.png"
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 48
            width: parent.width * 0.8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "white"
            font.pixelSize: 20
            text: qsTr("ZionOS is being installed.\nThe system is based on Debian and receives Debian security updates.")
        }
    }

    function onActivate() { }
    function onLeave() { }
}
