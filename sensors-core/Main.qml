import QtQuick
import QtQuick.Window
import QtWebEngine
import QtWebView
// import Qt5Compat.GraphicalEffects
import QtQuick.Controls

Window {
    width: Screen.width / 1.5
    height: Screen.height / 1.5
    visible: true
    title: webView.title ? webView.title: "Web Browser"

    Rectangle {
        id: header
        height: 60
        color: "#2c3e50"

        Text {
            text: webView.title
            color: "white"
            anchors.centerIn: parent
        }
    }

    WebEngineView {
        id: webView
        anchors.fill: parent
        url: "http://localhost:3000"

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineLoadRequest.LoadStartedStatus) {
                console.log("Loading started:", loadRequest.url)
            } else if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus) {
                console.log("Loading succeeded:", loadRequest.url)
            } else if (loadRequest.status === WebEngineLoadRequest.LoadFailedStatus) {
                console.error("Loading failed:", loadRequest.url, "Error:", loadRequest.errorString)
            }
        }

        onTitleChanged: {
            console.log("Page title:", title)
        }

        Behavior on opacity {
            NumberAnimation { duration: 3000 }
        }
    }

    // Floating controls
    RoundButton {
        id: reloadButton
        text: "↻"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 60
        height: 60
        font.pixelSize: 20
        opacity: 0.8

        // Nice shadow effect
        // layer.enabled: true
        // layer.effect: DropShadow {
        //     verticalOffset: 3
        //     color: "#80000000"
        //     radius: 8
        //     samples: 16
        // }

        onClicked: webView.reload()
    }

    // Progress bar for loading
    Rectangle {
        id: progressBar
        anchors.top: parent.top
        width: parent.width * webView.loadProgress / 100
        height: 3
        color: "blue"
        visible: webView.loading
    }


    // Опционально: кнопка для закрытия приложения
    RoundButton {
        width: 40
        height: 40
        // text: "↻"
        // color: "red"
        radius: 20
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
       opacity: 0.8

        Text {
            text: "↻"
            color: "red"
            anchors.centerIn: parent
            font.pixelSize: 20
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: webView.reload()
        }
    }
}
