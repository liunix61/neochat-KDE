// SPDX-FileCopyrightText: 2022 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "completionmodel.h"
#include <QDebug>

#include "actionsmodel.h"
#include "chatdocumenthandler.h"
#include "completionproxymodel.h"
#include "customemojimodel.h"
#include "emojimodel.h"
#include "neochatroom.h"
#include "roomlistmodel.h"
#include "userlistmodel.h"

CompletionModel::CompletionModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_filterModel(new CompletionProxyModel())
    , m_userListModel(new UserListModel(this))
    , m_emojiModel(new KConcatenateRowsProxyModel(this))
{
    connect(this, &CompletionModel::textChanged, this, &CompletionModel::updateCompletion);
    connect(this, &CompletionModel::roomChanged, this, [this]() {
        m_userListModel->setRoom(m_room);
    });
    m_emojiModel->addSourceModel(&CustomEmojiModel::instance());
    m_emojiModel->addSourceModel(&EmojiModel::instance());
}

QString CompletionModel::text() const
{
    return m_text;
}

void CompletionModel::setText(const QString &text, const QString &fullText)
{
    m_text = text;
    m_fullText = fullText;
    Q_EMIT textChanged();
}

int CompletionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (m_autoCompletionType == None) {
        return 0;
    }
    return m_filterModel->rowCount();
}

QVariant CompletionModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_filterModel->rowCount()) {
        return {};
    }
    auto filterIndex = m_filterModel->index(index.row(), 0);
    if (m_autoCompletionType == User) {
        if (role == Text) {
            return m_filterModel->data(filterIndex, UserListModel::NameRole);
        }
        if (role == Subtitle) {
            return m_filterModel->data(filterIndex, UserListModel::UserIdRole);
        }
        if (role == Icon) {
            return m_filterModel->data(filterIndex, UserListModel::AvatarRole);
        }
    }

    if (m_autoCompletionType == Command) {
        if (role == Text) {
            return m_filterModel->data(filterIndex, ActionsModel::Prefix).toString() + QStringLiteral(" ")
                + m_filterModel->data(filterIndex, ActionsModel::Parameters).toString();
        }
        if (role == Subtitle) {
            return m_filterModel->data(filterIndex, ActionsModel::Description);
        }
        if (role == Icon) {
            return QStringLiteral("invalid");
        }
        if (role == ReplacedText) {
            return m_filterModel->data(filterIndex, ActionsModel::Prefix);
        }
    }
    if (m_autoCompletionType == Room) {
        if (role == Text) {
            return m_filterModel->data(filterIndex, RoomListModel::DisplayNameRole);
        }
        if (role == Subtitle) {
            return m_filterModel->data(filterIndex, RoomListModel::CanonicalAliasRole);
        }
        if (role == Icon) {
            return m_filterModel->data(filterIndex, RoomListModel::AvatarRole);
        }
    }
    if (m_autoCompletionType == Emoji) {
        if (role == Text) {
            return m_filterModel->data(filterIndex, CustomEmojiModel::DisplayRole);
        }
        if (role == Icon) {
            return m_filterModel->data(filterIndex, CustomEmojiModel::MxcUrl);
        }
        if (role == ReplacedText) {
            return m_filterModel->data(filterIndex, CustomEmojiModel::ReplacedTextRole);
        }
        if (role == Subtitle) {
            return m_filterModel->data(filterIndex, EmojiModel::DescriptionRole);
        }
    }

    return {};
}

QHash<int, QByteArray> CompletionModel::roleNames() const
{
    return {
        {Text, "text"},
        {Subtitle, "subtitle"},
        {Icon, "icon"},
        {ReplacedText, "replacedText"},
    };
}

void CompletionModel::updateCompletion()
{
    if (text().startsWith(QLatin1Char('@'))) {
        m_filterModel->setSourceModel(m_userListModel);
        m_filterModel->setFilterRole(UserListModel::UserIdRole);
        m_filterModel->setSecondaryFilterRole(UserListModel::NameRole);
        m_filterModel->setFullText(m_fullText);
        m_filterModel->setFilterText(m_text);
        m_autoCompletionType = User;
        m_filterModel->invalidate();
    } else if (text().startsWith(QLatin1Char('/'))) {
        m_filterModel->setSourceModel(&ActionsModel::instance());
        m_filterModel->setFilterRole(ActionsModel::Prefix);
        m_filterModel->setSecondaryFilterRole(-1);
        m_filterModel->setFullText(m_fullText);
        m_filterModel->setFilterText(m_text.mid(1));
        m_autoCompletionType = Command;
        m_filterModel->invalidate();
    } else if (text().startsWith(QLatin1Char('#'))) {
        m_autoCompletionType = Room;
        m_filterModel->setSourceModel(m_roomListModel);
        m_filterModel->setFilterRole(RoomListModel::CanonicalAliasRole);
        m_filterModel->setSecondaryFilterRole(RoomListModel::DisplayNameRole);
        m_filterModel->setFullText(m_fullText);
        m_filterModel->setFilterText(m_text);
        m_filterModel->invalidate();
    } else if (text().startsWith(QLatin1Char(':'))
               && (m_fullText.indexOf(QLatin1Char(':'), 1) == -1
                   || (m_fullText.indexOf(QLatin1Char(' ')) != -1 && m_fullText.indexOf(QLatin1Char(':'), 1) > m_fullText.indexOf(QLatin1Char(' '), 1)))) {
        m_filterModel->setSourceModel(m_emojiModel);
        m_autoCompletionType = Emoji;
        m_filterModel->setFilterRole(CustomEmojiModel::Name);
        m_filterModel->setSecondaryFilterRole(EmojiModel::DescriptionRole);
        m_filterModel->setFullText(m_fullText);
        m_filterModel->setFilterText(m_text);
        m_filterModel->invalidate();
    } else {
        m_autoCompletionType = None;
    }
    beginResetModel();
    endResetModel();
}

NeoChatRoom *CompletionModel::room() const
{
    return m_room;
}

void CompletionModel::setRoom(NeoChatRoom *room)
{
    m_room = room;
    Q_EMIT roomChanged();
}

CompletionModel::AutoCompletionType CompletionModel::autoCompletionType() const
{
    return m_autoCompletionType;
}

void CompletionModel::setAutoCompletionType(AutoCompletionType autoCompletionType)
{
    m_autoCompletionType = autoCompletionType;
    Q_EMIT autoCompletionTypeChanged();
}

RoomListModel *CompletionModel::roomListModel() const
{
    return m_roomListModel;
}

void CompletionModel::setRoomListModel(RoomListModel *roomListModel)
{
    m_roomListModel = roomListModel;
    Q_EMIT roomListModelChanged();
}
