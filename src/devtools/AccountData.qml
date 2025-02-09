// SPDX-FileCopyrightText: 2024 Tobias Fella <tobias.fella@kde.org>
// SPDX-License-Identifier: LGPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import org.kde.neochat

ColumnLayout {
    id: root

    required property NeoChatConnection connection

    FormCard.FormHeader {
        title: i18nc("@title:group", "Account Data")
    }
    FormCard.FormCard {
        Repeater {
            model: root.connection.accountDataEventTypes
            delegate: FormCard.FormButtonDelegate {
                text: modelData
                onClicked: root.Window.window.pageStack.pushDialogLayer(Qt.createComponent('org.kde.neochat', 'MessageSourceSheet'), {
                    sourceText: root.connection.accountDataJsonString(modelData)
                }, {
                    title: i18nc("@title:window", "Event Source"),
                    width: Kirigami.Units.gridUnit * 25
                })
            }
        }
    }
}
