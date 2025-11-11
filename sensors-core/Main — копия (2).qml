import QtQuick
import QtQuick.Window

Window {
    width: Screen.width
    height: Screen.height
    visible: true
    title: qsTr("Web Browser")
    flags: Qt.Window

    // Опционально: кнопка для закрытия приложения
    Rectangle {
        width: 40
        height: 40
        color: "red"
        radius: 20
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10

        Text {
            text: "X"
            color: "white"
            anchors.centerIn: parent
            font.pixelSize: 16
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: Qt.quit()
        }
    }
}
