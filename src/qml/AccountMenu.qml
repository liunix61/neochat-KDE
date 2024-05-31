// SPDX-FileCopyrightText: 2022 James Graham <james.h.graham@protonmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.neochat
import org.kde.neochat.settings
import org.kde.neochat.devtools

QQC2.Menu {
    id: root

    required property NeoChatConnection connection
    required property Kirigami.ApplicationWindow window

    margins: Kirigami.Units.smallSpacing

    QQC2.MenuItem {
        text: i18n("Edit this account")
        icon.name: "document-edit"
        onTriggered: root.window.pageStack.pushDialogLayer(Qt.createComponent('org.kde.neochat.settings', 'AccountEditorPage'), {
            connection: root.connection
        }, {
            title: i18n("Account editor")
        })
    }
    QQC2.MenuItem {
        text: i18n("Notification settings")
        icon.name: "notifications"
        onTriggered: {
            NeoChatSettingsView.open('notifications');
        }
    }
    QQC2.MenuItem {
        text: i18n("Devices")
        icon.name: "computer-symbolic"
        onTriggered: {
            NeoChatSettingsView.open('devices');
        }
    }
    QQC2.MenuItem {
        text: i18n("Open developer tools")
        icon.name: "tools"
        visible: Config.developerTools
        onTriggered: root.window.pageStack.pushDialogLayer(Qt.createComponent('org.kde.neochat.devtools', 'DevtoolsPage'), {
            connection: root.connection
        }, {
            title: i18nc("@title:window", "Developer Tools"),
            width: Kirigami.Units.gridUnit * 50,
            height: Kirigami.Units.gridUnit * 42
        })
    }
    QQC2.MenuItem {
        text: i18nc("@action:inmenu", "Open Secret Backup")
        icon.name: "unlock"
        visible: Config.secretBackup
        onTriggered: root.window.pageStack.pushDialogLayer(Qt.createComponent('org.kde.neochat', 'UnlockSSSSDialog'), {}, {
            title: i18nc("@title:window", "Open Key Backup")
        })
    }
    QQC2.MenuItem {
        text: i18nc("@action:inmenu", "Verify this Device")
        icon.name: "security-low"
        onTriggered: root.connection.startSelfVerification()
        enabled: Controller.csSupported
    }
    QQC2.MenuItem {
        text: i18n("Logout")
        icon.name: "list-remove-user"
        onTriggered: confirmLogoutDialogComponent.createObject(root.window.overlay).open()
    }

    Component {
        id: confirmLogoutDialogComponent
        ConfirmLogoutDialog {
            connection: root.connection
        }
    }
}
