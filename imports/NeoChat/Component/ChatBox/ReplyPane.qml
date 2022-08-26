// SPDX-FileCopyrightText: 2020 Carl Schwan <carl@carlschwan.de>
// SPDX-FileCopyrightText: 2020 Noah Davis <noahadvs@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import org.kde.kirigami 2.14 as Kirigami

import org.kde.neochat 1.0

Loader {
    id: root
    readonly property bool isEdit: chatBoxHelper.isEditing
    property var user: null
    property string avatarMediaUrl: user ? "image://mxc/" + user.avatarMediaId : ""

    signal replyCancelled()

    active: visible
    sourceComponent: Pane {
        id: replyPane

        Kirigami.Theme.colorSet: Kirigami.Theme.View

        spacing: leftPadding

        contentItem: RowLayout {
            Layout.fillWidth: true
            spacing: replyPane.spacing

            FontMetrics {
                id: fontMetrics
                font: textArea.font
            }

            Kirigami.Avatar {
                id: avatar
                Layout.alignment: textContentLayout.height > avatar.height ? Qt.AlignHCenter | Qt.AlignTop : Qt.AlignCenter
                Layout.preferredWidth: Layout.preferredHeight
                Layout.preferredHeight: fontMetrics.lineSpacing * 2 - fontMetrics.leading
                source: root.avatarMediaUrl
                name: user ? user.displayName : ""
                color: user ? user.color : "transparent"
                visible: Boolean(user)
            }

            ColumnLayout {
                id: textContentLayout
                Layout.alignment: Qt.AlignCenter
                Layout.fillWidth: true
                spacing: fontMetrics.leading
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.StyledText
                    elide: Text.ElideRight
                    text: {
                        let heading = "<b>%1</b>"
                        let userName = user ? "<font color=\""+ user.color +"\">" + currentRoom.htmlSafeMemberName(user.id) + "</font>" : ""
                        if (isEdit) {
                            heading = heading.arg(i18n("Editing message:")) + "<br/>"
                        } else {
                            heading = heading.arg(i18n("Replying to %1:", userName))
                        }

                        return heading
                    }
                }
                ScrollView {
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    Layout.fillWidth: true
                    Layout.maximumHeight: fontMetrics.lineSpacing * 8 - fontMetrics.leading

                    // HACK: Hide unnecessary horizontal scrollbar (https://bugreports.qt.io/browse/QTBUG-83890)
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    TextArea {
                        id: textArea
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0
                        bottomPadding: 0
                        text: {
                            const stylesheet = "<style> a{color:"+Kirigami.Theme.linkColor+";}.user-pill{}</style>";
                            const content = chatBoxHelper.isReplying ? chatBoxHelper.replyEventContent : chatBoxHelper.editContent;
                            return stylesheet + content;
                        }
                        selectByMouse: true
                        selectByKeyboard: true
                        readOnly: true
                        wrapMode: Label.Wrap
                        textFormat: TextEdit.RichText
                        background: Item {}
                        HoverHandler {
                            cursorShape: textArea.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                        }
                    }
                }
            }

            Button {
                id: cancelReplyButton
                Layout.alignment: avatar.Layout.alignment
                icon.name: "dialog-cancel"
                text: i18n("Cancel")
                display: AbstractButton.IconOnly
                onClicked: {
                    chatBoxHelper.clear();
                    root.replyCancelled();
                }
                ToolTip.text: text
                ToolTip.visible: hovered
            }
        }

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }
    }
}
