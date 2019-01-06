import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

Attachment {
    property var sourceSize
    property url source
    property var maxHeight
    property bool autoload

    Image {
        readonly property real imageMaxHeight: maxHeight - buttons.height

        id: imageContent
        width: parent.width
        height: sourceSize.height *
                Math.min(imageMaxHeight / sourceSize.height * 0.9,
                         Math.min(width / sourceSize.width, 1))
        fillMode: Image.PreserveAspectFit

        source: parent.source
        sourceSize: parent.sourceSize

//        Behavior on height { NumberAnimation {
//            duration: settings.fast_animations_duration_ms
//            easing.type: Easing.OutQuad
//        }}

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onContainsMouseChanged:
                controller.showStatusMessage(containsMouse
                                             ? room.fileSource(eventId) : "")
            onClicked: openExternally()
        }

        Component.onCompleted:
            if (visible && autoload && !(progressInfo && progressInfo.isUpload))
                room.downloadFile(eventId)
    }

    RowLayout {
        id: buttons
        anchors.top: imageContent.bottom
        width: parent.width
        spacing: 2

        Button {
            text: qsTr("Cancel")
            visible: progressInfo.started
            onClicked: room.cancelFileTransfer(eventId)
        }

        Button {
            text: qsTr("Open externally")
            onClicked: openExternally()
        }
        Button {
            text: qsTr("Download full size")
            visible: !autoload && !progressInfo.active

            onClicked: room.downloadFile(eventId)
        }
        Button {
            text: qsTr("Save as...")
            onClicked: controller.saveFileAs(eventId)
        }
    }
}
