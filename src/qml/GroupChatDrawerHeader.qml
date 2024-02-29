// SPDX-FileCopyrightText: 2020 Carl Schwan <carl@carlschwan.eu>
// SPDX-FileCopyrightText: 2023 Tobias Fella <tobias.fella@kde.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.labs.components as KirigamiComponents

import org.kde.neochat

ColumnLayout {
    id: root
    /**
     * @brief The current room that user is viewing.
     */
    required property NeoChatRoom room

    Layout.fillWidth: true

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.largeSpacing

        spacing: Kirigami.Units.largeSpacing

        KirigamiComponents.Avatar {
            Layout.preferredWidth: Kirigami.Units.iconSizes.large
            Layout.preferredHeight: Kirigami.Units.iconSizes.large

            name: root.room ? root.room.displayName : ""
            source: root.room && root.room.avatarMediaId ? ("image://mxc/" + root.room.avatarMediaId) : ""

            Rectangle {
                visible: room.usesEncryption
                color: Kirigami.Theme.backgroundColor

                width: Kirigami.Units.gridUnit
                height: Kirigami.Units.gridUnit

                anchors {
                    bottom: parent.bottom
                    right: parent.right
                }

                radius: Math.round(width / 2)

                Kirigami.Icon {
                    source: "channel-secure-symbolic"
                    anchors.fill: parent
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 0

            Kirigami.Heading {
                Layout.fillWidth: true
                text: root.room ? root.room.displayName : i18n("No name")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }

            Kirigami.SelectableLabel {
                Layout.fillWidth: true
                font: Kirigami.Theme.smallFont
                textFormat: TextEdit.PlainText
                visible: root.room && root.room.canonicalAlias
                text: root.room && root.room.canonicalAlias ? root.room.canonicalAlias : ""
            }
        }
    }

    Kirigami.SelectableLabel {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing

        visible: text.length > 0

        text: root.room && root.room.topic ? root.room.topic : ""
        textFormat: TextEdit.MarkdownText
        wrapMode: Text.Wrap
        onLinkActivated: link => UrlHelper.openUrl(link)
    }
}
