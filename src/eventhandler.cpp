// SPDX-FileCopyrightText: 2023 James Graham <james.h.graham@protonmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "eventhandler.h"

#include <QMovie>

#include <KFormat>
#include <KLocalizedString>

#include <Quotient/events/encryptionevent.h>
#include <Quotient/events/event.h>
#include <Quotient/events/eventcontent.h>
#include <Quotient/events/reactionevent.h>
#include <Quotient/events/redactionevent.h>
#include <Quotient/events/roomavatarevent.h>
#include <Quotient/events/roomcanonicalaliasevent.h>
#include <Quotient/events/roomevent.h>
#include <Quotient/events/roommemberevent.h>
#include <Quotient/events/roompowerlevelsevent.h>
#include <Quotient/events/simplestateevents.h>
#include <Quotient/events/stickerevent.h>
#include <Quotient/quotient_common.h>
#include <Quotient/roommember.h>

#include "eventhandler_logging.h"
#include "events/locationbeaconevent.h"
#include "events/pollevent.h"
#include "events/widgetevent.h"
#include "neochatconfig.h"
#include "neochatroom.h"
#include "texthandler.h"
#include "utils.h"

using namespace Quotient;

namespace
{
enum MemberChange {
    None = 0,
    AddName = 1,
    Rename = 2,
    RemoveName = 4,
    AddAvatar = 8,
    UpdateAvatar = 16,
    RemoveAvatar = 32,
};
Q_DECLARE_FLAGS(MemberChanges, MemberChange)
Q_DECLARE_OPERATORS_FOR_FLAGS(MemberChanges)
};

QString EventHandler::id(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "id called with event set to nullptr.";
        return {};
    }

    return !event->id().isEmpty() ? event->id() : event->transactionId();
}

QString EventHandler::authorDisplayName(const NeoChatRoom *room, const Quotient::RoomEvent *event, bool isPending)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "authorDisplayName called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "authorDisplayName called with event set to nullptr.";
        return {};
    }

    if (is<RoomMemberEvent>(*event) && !event->unsignedJson()[QStringLiteral("prev_content")][QStringLiteral("displayname")].isNull()
        && event->stateKey() == event->senderId()) {
        auto previousDisplayName = event->unsignedJson()[QStringLiteral("prev_content")][QStringLiteral("displayname")].toString().toHtmlEscaped();
        if (previousDisplayName.isEmpty()) {
            previousDisplayName = event->senderId();
        }
        return previousDisplayName;
    } else {
        const auto author = isPending ? room->localMember() : room->member(event->senderId());
        return author.htmlSafeDisplayName();
    }
}

QString EventHandler::singleLineAuthorDisplayname(const NeoChatRoom *room, const Quotient::RoomEvent *event, bool isPending)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "singleLineAuthorDisplayname called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "singleLineAuthorDisplayname called with event set to nullptr.";
        return {};
    }

    const auto author = isPending ? room->localMember() : room->member(event->senderId());
    auto displayName = author.displayName();
    displayName.replace(QStringLiteral("<br>\n"), QStringLiteral(" "));
    displayName.replace(QStringLiteral("<br>"), QStringLiteral(" "));
    displayName.replace(QStringLiteral("<br />\n"), QStringLiteral(" "));
    displayName.replace(QStringLiteral("<br />"), QStringLiteral(" "));
    displayName.replace(u'\n', QStringLiteral(" "));
    displayName.replace(u'\u2028', QStringLiteral(" "));
    return displayName;
}

QDateTime EventHandler::time(const Quotient::RoomEvent *event, bool isPending, QDateTime lastUpdated)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "time called with event set to nullptr.";
        return {};
    }
    if (isPending && lastUpdated == QDateTime()) {
        qCWarning(EventHandling) << "a value must be provided for lastUpdated for a pending event.";
        return {};
    }

    return isPending ? lastUpdated : event->originTimestamp();
}

QString EventHandler::timeString(const Quotient::RoomEvent *event, bool relative, QLocale::FormatType format, bool isPending, QDateTime lastUpdated)
{
    auto ts = time(event, isPending, lastUpdated);
    if (ts.isValid()) {
        if (relative) {
            KFormat formatter;
            return formatter.formatRelativeDate(ts.toLocalTime().date(), format);
        } else {
            return QLocale().toString(ts.toLocalTime().time(), format);
        }
    }
    return {};
}

QString EventHandler::timeString(const Quotient::RoomEvent *event, const QString &format, bool isPending, const QDateTime &lastUpdated)
{
    return time(event, isPending, lastUpdated).toLocalTime().toString(format);
}

bool EventHandler::isHighlighted(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "isHighlighted called with room set to nullptr.";
        return false;
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "isHighlighted called with event set to nullptr.";
        return false;
    }

    return !room->isDirectChat() && room->isEventHighlighted(event);
}

bool EventHandler::isHidden(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "isHidden called with room set to nullptr.";
        return false;
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "isHidden called with event set to nullptr.";
        return false;
    }

    if (event->isStateEvent() && !NeoChatConfig::self()->showStateEvent()) {
        return true;
    }

    if (auto roomMemberEvent = eventCast<const RoomMemberEvent>(event)) {
        if ((roomMemberEvent->isJoin() || roomMemberEvent->isLeave()) && !NeoChatConfig::self()->showLeaveJoinEvent()) {
            return true;
        } else if (roomMemberEvent->isRename() && roomMemberEvent->prevContent() && roomMemberEvent->prevContent()->membership == roomMemberEvent->membership()
                   && !NeoChatConfig::self()->showRename()) {
            return true;
        } else if (roomMemberEvent->isAvatarUpdate() && !roomMemberEvent->isJoin() && !roomMemberEvent->isLeave()
                   && !NeoChatConfig::self()->showAvatarUpdate()) {
            return true;
        }
    }

    if (event->isStateEvent() && eventCast<const StateEvent>(event)->repeatsState()) {
        return true;
    }

    // isReplacement?
    if (auto e = eventCast<const RoomMessageEvent>(event)) {
        if (!e->replacedEvent().isEmpty()) {
            return true;
        }
    }

    if (is<RedactionEvent>(*event) || is<ReactionEvent>(*event)) {
        return true;
    }

    if (auto e = eventCast<const RoomMessageEvent>(event)) {
        if (!e->replacedEvent().isEmpty() && e->replacedEvent() != e->id()) {
            return true;
        }
    }

    if (room->connection()->isIgnored(event->senderId())) {
        return true;
    }

    // hide ending live location beacons
    if (event->isStateEvent() && event->matrixType() == "org.matrix.msc3672.beacon_info"_ls && !event->contentJson()["live"_ls].toBool()) {
        return true;
    }

    return false;
}

Qt::TextFormat EventHandler::messageBodyInputFormat(const Quotient::RoomMessageEvent &event)
{
    if (event.mimeType().name() == "text/plain"_ls) {
        return Qt::PlainText;
    } else {
        return Qt::RichText;
    }
}

QString EventHandler::rawMessageBody(const Quotient::RoomMessageEvent &event)
{
    QString body;

    if (event.has<EventContent::FileContent>()) {
        // if filename is given or body is equal to filename,
        // then body is a caption
        QString filename = event.get<EventContent::FileContent>()->originalName;
        QString body = event.plainBody();
        if (filename.isEmpty() || filename == body) {
            return QString();
        }
        return body;
    }

    if (event.has<EventContent::TextContent>() && event.content()) {
        body = event.get<EventContent::TextContent>()->body;
    } else {
        body = event.plainBody();
    }
    return body;
}

QString EventHandler::richBody(const NeoChatRoom *room, const Quotient::RoomEvent *event, bool stripNewlines)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "richBody called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "richBody called with event set to nullptr.";
        return {};
    }
    return getBody(room, event, Qt::RichText, stripNewlines);
}

QString EventHandler::plainBody(const NeoChatRoom *room, const Quotient::RoomEvent *event, bool stripNewlines)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "plainBody called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "plainBody called with event set to nullptr.";
        return {};
    }
    return getBody(room, event, Qt::PlainText, stripNewlines);
}

QString EventHandler::markdownBody(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "markdownBody called with event set to nullptr.";
        return {};
    }

    if (!event->is<RoomMessageEvent>()) {
        qCWarning(EventHandling) << "markdownBody called when event isn't a RoomMessageEvent.";
        return {};
    }

    const auto roomMessageEvent = eventCast<const RoomMessageEvent>(event);

    QString plainBody = roomMessageEvent->plainBody();
    plainBody.remove(TextRegex::removeReply);
    return plainBody;
}

QString EventHandler::getBody(const NeoChatRoom *room, const Quotient::RoomEvent *event, Qt::TextFormat format, bool stripNewlines)
{
    if (event->isRedacted()) {
        auto reason = event->redactedBecause()->reason();
        return (reason.isEmpty()) ? i18n("<i>[This message was deleted]</i>") : i18n("<i>[This message was deleted: %1]</i>", reason.toHtmlEscaped());
    }

    const bool prettyPrint = (format == Qt::RichText);

    return switchOnType(
        *event,
        [room, format, stripNewlines](const RoomMessageEvent &event) {
            return getMessageBody(room, event, format, stripNewlines);
        },
        [](const StickerEvent &e) {
            return e.body();
        },
        [room, prettyPrint](const RoomMemberEvent &e) {
            // FIXME: Rewind to the name that was at the time of this event
            auto subjectName = prettyPrint ? room->member(e.userId()).htmlSafeDisplayName() : room->member(e.userId()).displayName();
            if (e.membership() == Membership::Leave) {
                if (e.prevContent() && e.prevContent()->displayName) {
                    subjectName = sanitized(*e.prevContent()->displayName);
                    if (prettyPrint) {
                        subjectName = subjectName.toHtmlEscaped();
                    }
                }
            }

            if (prettyPrint) {
                subjectName = QStringLiteral("<a href=\"https://matrix.to/#/%1\" style=\"color: %2\">%3</a>")
                                  .arg(e.userId(), room->member(e.userId()).color().name(), subjectName);
            }

            // The below code assumes senderName output in AuthorRole
            switch (e.membership()) {
            case Membership::Invite:
                if (e.repeatsState()) {
                    auto text = i18n("reinvited %1 to the room", subjectName);
                    if (!e.reason().isEmpty()) {
                        text += i18nc("Optional reason for an invitation", ": %1") + (prettyPrint ? e.reason().toHtmlEscaped() : e.reason());
                    }
                    return text;
                }
                Q_FALLTHROUGH();
            case Membership::Join: {
                QString text{};
                // Part 1: invites and joins
                if (e.repeatsState()) {
                    text = i18n("joined the room (repeated)");
                } else if (e.changesMembership()) {
                    text = e.membership() == Membership::Invite ? i18n("invited %1 to the room", subjectName) : i18n("joined the room");
                }
                if (!text.isEmpty()) {
                    if (!e.reason().isEmpty()) {
                        text += i18n(": %1", e.reason().toHtmlEscaped());
                    }
                    return text;
                }
                // Part 2: profile changes of joined members
                if (e.isRename()) {
                    if (!e.newDisplayName()) {
                        text = i18nc("their refers to a singular user", "cleared their display name");
                    } else {
                        text = i18nc("their refers to a singular user",
                                     "changed their display name to %1",
                                     prettyPrint ? e.newDisplayName()->toHtmlEscaped() : *e.newDisplayName());
                    }
                }
                if (e.isAvatarUpdate()) {
                    if (!text.isEmpty()) {
                        text += i18n(" and ");
                    }
                    if (!e.newAvatarUrl()) {
                        text += i18nc("their refers to a singular user", "cleared their avatar");
                    } else if (!e.prevContent()->avatarUrl) {
                        text += i18n("set an avatar");
                    } else {
                        text += i18nc("their refers to a singular user", "updated their avatar");
                    }
                }
                if (text.isEmpty()) {
                    text = i18nc("<user> changed nothing", "changed nothing");
                }
                return text;
            }
            case Membership::Leave:
                if (e.prevContent() && e.prevContent()->membership == Membership::Invite) {
                    return (e.senderId() != e.userId()) ? i18n("withdrew %1's invitation", subjectName) : i18n("rejected the invitation");
                }

                if (e.prevContent() && e.prevContent()->membership == Membership::Ban) {
                    return (e.senderId() != e.userId()) ? i18n("unbanned %1", subjectName) : i18n("self-unbanned");
                }
                if (e.senderId() == e.userId()) {
                    return i18n("left the room");
                }
                if (const auto &reason = e.contentJson()["reason"_ls].toString().toHtmlEscaped(); !reason.isEmpty()) {
                    return i18n("has put %1 out of the room: %2", subjectName, reason);
                }
                return i18n("has put %1 out of the room", subjectName);
            case Membership::Ban:
                if (e.senderId() != e.userId()) {
                    if (e.reason().isEmpty()) {
                        return i18n("banned %1 from the room", subjectName);
                    } else {
                        return i18n("banned %1 from the room: %2", subjectName, prettyPrint ? e.reason().toHtmlEscaped() : e.reason());
                    }
                } else {
                    return i18n("self-banned from the room");
                }
            case Membership::Knock: {
                QString reason(e.contentJson()["reason"_ls].toString().toHtmlEscaped());
                return reason.isEmpty() ? i18n("requested an invite") : i18n("requested an invite with reason: %1", reason);
            }
            default:;
            }
            return i18n("made something unknown");
        },
        [](const RoomCanonicalAliasEvent &e) {
            return (e.alias().isEmpty()) ? i18n("cleared the room main alias") : i18n("set the room main alias to: %1", e.alias());
        },
        [prettyPrint](const RoomNameEvent &e) {
            return (e.name().isEmpty()) ? i18n("cleared the room name") : i18n("set the room name to: %1", prettyPrint ? e.name().toHtmlEscaped() : e.name());
        },
        [prettyPrint, stripNewlines](const RoomTopicEvent &e) {
            return (e.topic().isEmpty()) ? i18n("cleared the topic")
                                         : i18n("set the topic to: %1",
                                                prettyPrint         ? Quotient::prettyPrint(e.topic())
                                                    : stripNewlines ? e.topic().replace(u'\n', u' ')
                                                                    : e.topic());
        },
        [](const RoomAvatarEvent &) {
            return i18n("changed the room avatar");
        },
        [](const EncryptionEvent &) {
            return i18n("activated End-to-End Encryption");
        },
        [prettyPrint](const RoomCreateEvent &e) {
            return e.isUpgrade()
                ? i18n("upgraded the room to version %1", e.version().isEmpty() ? "1"_ls : (prettyPrint ? e.version().toHtmlEscaped() : e.version()))
                : i18n("created the room, version %1", e.version().isEmpty() ? "1"_ls : (prettyPrint ? e.version().toHtmlEscaped() : e.version()));
        },
        [](const RoomPowerLevelsEvent &) {
            return i18nc("'power level' means permission level", "changed the power levels for this room");
        },
        [](const LocationBeaconEvent &e) {
            return e.contentJson()["description"_ls].toString();
        },
        [](const RoomServerAclEvent &) {
            return i18n("changed the server access control lists for this room");
        },
        [](const WidgetEvent &e) {
            if (e.fullJson()["unsigned"_ls]["prev_content"_ls].toObject().isEmpty()) {
                return i18nc("[User] added <name> widget", "added %1 widget", e.contentJson()["name"_ls].toString());
            }
            if (e.contentJson().isEmpty()) {
                return i18nc("[User] removed <name> widget", "removed %1 widget", e.fullJson()["unsigned"_ls]["prev_content"_ls]["name"_ls].toString());
            }
            return i18nc("[User] configured <name> widget", "configured %1 widget", e.contentJson()["name"_ls].toString());
        },
        [prettyPrint](const StateEvent &e) {
            return e.stateKey().isEmpty() ? i18n("updated %1 state", e.matrixType())
                                          : i18n("updated %1 state for %2", e.matrixType(), prettyPrint ? e.stateKey().toHtmlEscaped() : e.stateKey());
        },
        [](const PollStartEvent &e) {
            return e.question();
        },
        i18n("Unknown event"));
}

QString EventHandler::getMessageBody(const NeoChatRoom *room, const RoomMessageEvent &event, Qt::TextFormat format, bool stripNewlines)
{
    TextHandler textHandler;

    if (event.has<EventContent::FileContent>()) {
        QString fileCaption = event.get<EventContent::FileContent>()->originalName;
        if (fileCaption.isEmpty()) {
            fileCaption = event.plainBody();
        } else if (fileCaption != event.plainBody()) {
            fileCaption = event.plainBody() + " | "_ls + fileCaption;
        }
        textHandler.setData(fileCaption);
        return !fileCaption.isEmpty() ? textHandler.handleRecievePlainText(Qt::PlainText, stripNewlines) : i18n("a file");
    }

    QString body;
    if (event.has<EventContent::TextContent>() && event.content()) {
        body = event.get<EventContent::TextContent>()->body;
    } else {
        body = event.plainBody();
    }

    textHandler.setData(body);

    Qt::TextFormat inputFormat;
    if (event.mimeType().name() == "text/plain"_ls) {
        inputFormat = Qt::PlainText;
    } else {
        inputFormat = Qt::RichText;
    }

    if (format == Qt::RichText) {
        return textHandler.handleRecieveRichText(inputFormat, room, &event, stripNewlines, event.isReplaced());
    } else {
        return textHandler.handleRecievePlainText(inputFormat, stripNewlines);
    }
}

QString EventHandler::genericBody(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "genericBody called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "genericBody called with event set to nullptr.";
        return {};
    }
    if (event->isRedacted()) {
        return i18n("<i>[This message was deleted]</i>");
    }

    const auto sender = room->member(event->senderId());
    const auto senderString = QStringLiteral("<a href=\"https://matrix.to/#/%1\">%2</a>").arg(sender.id(), sender.htmlSafeDisplayName());

    return switchOnType(
        *event,
        [senderString](const RoomMessageEvent &) {
            return i18n("%1 sent a message", senderString);
        },
        [senderString](const StickerEvent &) {
            return i18n("%1 sent a sticker", senderString);
        },
        [senderString](const RoomMemberEvent &e) {
            switch (e.membership()) {
            case Membership::Invite:
                if (e.repeatsState()) {
                    return i18n("%1 reinvited someone to the room", senderString);
                }
                Q_FALLTHROUGH();
            case Membership::Join: {
                // Part 1: invites and joins
                if (e.repeatsState()) {
                    return i18n("%1 joined the room (repeated)", senderString);
                } else if (e.changesMembership()) {
                    return e.membership() == Membership::Invite ? i18n("%1 invited someone to the room", senderString)
                                                                : i18n("%1 joined the room", senderString);
                }

                // Part 2: profile changes of joined members
                MemberChanges changes = None;
                if (e.isRename()) {
                    if (!e.newDisplayName()) {
                        changes |= RemoveName;
                    } else if (!e.prevContent()->displayName) {
                        changes |= AddName;
                    } else {
                        changes |= Rename;
                    }
                }
                if (e.isAvatarUpdate()) {
                    if (!e.newAvatarUrl()) {
                        changes |= RemoveAvatar;
                    } else if (!e.prevContent()->avatarUrl) {
                        changes |= AddAvatar;
                    } else {
                        changes |= UpdateAvatar;
                    }
                }

                if (changes.testFlag(AddName)) {
                    if (changes.testFlag(AddAvatar)) {
                        return i18n("%1 set a display name and set an avatar", senderString);
                    } else if (changes.testFlag(UpdateAvatar)) {
                        return i18n("%1 set a display name and updated their avatar", senderString);
                    } else if (changes.testFlag(RemoveAvatar)) {
                        return i18n("%1 set a display name and cleared their avatar", senderString);
                    }
                    return i18n("%1 set a display name for this room", senderString);
                } else if (changes.testFlag(Rename)) {
                    if (changes.testFlag(AddAvatar)) {
                        return i18n("%1 changed their display name and set an avatar", senderString);
                    } else if (changes.testFlag(UpdateAvatar)) {
                        return i18n("%1 changed their display name and updated their avatar", senderString);
                    } else if (changes.testFlag(RemoveAvatar)) {
                        return i18n("%1 changed their display name and cleared their avatar", senderString);
                    }
                    return i18n("%1 changed their display name", senderString);
                } else if (changes.testFlag(RemoveName)) {
                    if (changes.testFlag(AddAvatar)) {
                        return i18n("%1 cleared their display name and set an avatar", senderString);
                    } else if (changes.testFlag(UpdateAvatar)) {
                        return i18n("%1 cleared their display name and updated their avatar", senderString);
                    } else if (changes.testFlag(RemoveAvatar)) {
                        return i18n("%1 cleared their display name and cleared their avatar", senderString);
                    }
                    return i18n("%1 cleared their display name", senderString);
                }

                return i18nc("<user> changed nothing", "%1 changed nothing", senderString);
            }
            case Membership::Leave:
                if (e.prevContent() && e.prevContent()->membership == Membership::Invite) {
                    return (e.senderId() != e.userId()) ? i18n("%1 withdrew a user's invitation", senderString)
                                                        : i18n("%1 rejected the invitation", senderString);
                }

                if (e.prevContent() && e.prevContent()->membership == Membership::Ban) {
                    return (e.senderId() != e.userId()) ? i18n("%1 unbanned a user", senderString) : i18n("%1 self-unbanned", senderString);
                }
                return (e.senderId() != e.userId()) ? i18n("%1 put a user out of the room", senderString) : i18n("%1 left the room", senderString);
            case Membership::Ban:
                if (e.senderId() != e.userId()) {
                    return i18n("%1 banned a user from the room", senderString);
                } else {
                    return i18n("%1 self-banned from the room", senderString);
                }
            case Membership::Knock: {
                return i18n("%1 requested an invite", senderString);
            }
            default:;
            }
            return i18n("%1 made something unknown", senderString);
        },
        [senderString](const RoomCanonicalAliasEvent &e) {
            return (e.alias().isEmpty()) ? i18n("%1 cleared the room main alias", senderString) : i18n("%1 set the room main alias", senderString);
        },
        [senderString](const RoomNameEvent &e) {
            return (e.name().isEmpty()) ? i18n("%1 cleared the room name", senderString) : i18n("%1 set the room name", senderString);
        },
        [senderString](const RoomTopicEvent &e) {
            return (e.topic().isEmpty()) ? i18n("%1 cleared the topic", senderString) : i18n("%1 set the topic", senderString);
        },
        [senderString](const RoomAvatarEvent &) {
            return i18n("%1 changed the room avatar", senderString);
        },
        [senderString](const EncryptionEvent &) {
            return i18n("%1 activated End-to-End Encryption", senderString);
        },
        [senderString](const RoomCreateEvent &e) {
            return e.isUpgrade() ? i18n("%1 upgraded the room version", senderString) : i18n("%1 created the room", senderString);
        },
        [senderString](const RoomPowerLevelsEvent &) {
            return i18nc("'power level' means permission level", "%1 changed the power levels for this room", senderString);
        },
        [senderString](const LocationBeaconEvent &) {
            return i18n("%1 sent a live location beacon", senderString);
        },
        [senderString](const RoomServerAclEvent &) {
            return i18n("%1 changed the server access control lists for this room", senderString);
        },
        [senderString](const WidgetEvent &e) {
            if (e.fullJson()["unsigned"_ls]["prev_content"_ls].toObject().isEmpty()) {
                return i18n("%1 added a widget", senderString);
            }
            if (e.contentJson().isEmpty()) {
                return i18n("%1 removed a widget", senderString);
            }
            return i18n("%1 configured a widget", senderString);
        },
        [senderString](const StateEvent &) {
            return i18n("%1 updated the state", senderString);
        },
        [senderString](const PollStartEvent &) {
            return i18n("%1 started a poll", senderString);
        },
        i18n("Unknown event"));
}

QString EventHandler::subtitleText(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "subtitleText called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "subtitleText called with event set to nullptr.";
        return {};
    }
    return singleLineAuthorDisplayname(room, event) + (event->isStateEvent() ? QLatin1String(" ") : QLatin1String(": ")) + plainBody(room, event, true);
}

QVariantMap EventHandler::mediaInfo(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "mediaInfo called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "mediaInfo called with event set to nullptr.";
        return {};
    }
    return getMediaInfoForEvent(room, event);
}

QVariantMap EventHandler::getMediaInfoForEvent(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    QString eventId = event->id();

    // Get the file info for the event.
    if (event->is<RoomMessageEvent>()) {
        auto roomMessageEvent = eventCast<const RoomMessageEvent>(event);
        if (!roomMessageEvent->has<EventContent::FileContentBase>()) {
            return {};
        }

        const auto content = roomMessageEvent->get<EventContent::FileContentBase>();
        QVariantMap mediaInfo = getMediaInfoFromFileInfo(room, content.get(), eventId, false, false);
        // if filename isn't specifically given, it is in body
        // https://spec.matrix.org/latest/client-server-api/#mfile
        mediaInfo["filename"_ls] = content->commonInfo().originalName.isEmpty() ? roomMessageEvent->plainBody() : content->commonInfo().originalName;

        return mediaInfo;
    } else if (event->is<StickerEvent>()) {
        auto stickerEvent = eventCast<const StickerEvent>(event);
        auto content = &stickerEvent->image();

        return getMediaInfoFromFileInfo(room, content, eventId, false, true);
    } else {
        return {};
    }
}

QVariantMap EventHandler::getMediaInfoFromFileInfo(const NeoChatRoom *room,
                                                   const Quotient::EventContent::FileContentBase *fileContent,
                                                   const QString &eventId,
                                                   bool isThumbnail,
                                                   bool isSticker)
{
    QVariantMap mediaInfo;

    // Get the mxc URL for the media.
    if (!fileContent->url().isValid() || fileContent->url().scheme() != QStringLiteral("mxc") || eventId.isEmpty()) {
        mediaInfo["source"_ls] = QUrl();
    } else {
        QUrl source = room->makeMediaUrl(eventId, fileContent->url());

        if (source.isValid()) {
            mediaInfo["source"_ls] = source;
        } else {
            mediaInfo["source"_ls] = QUrl();
        }
    }

    auto mimeType = fileContent->type();
    // Add the MIME type for the media if available.
    mediaInfo["mimeType"_ls] = mimeType.name();

    // Add the MIME type icon if available.
    mediaInfo["mimeIcon"_ls] = mimeType.iconName();

    // Add media size if available.
    mediaInfo["size"_ls] = fileContent->commonInfo().payloadSize;

    mediaInfo["isSticker"_ls] = isSticker;

    // Add parameter depending on media type.
    if (mimeType.name().contains(QStringLiteral("image"))) {
        if (auto castInfo = static_cast<const EventContent::ImageContent *>(fileContent)) {
            mediaInfo["width"_ls] = castInfo->imageSize.width();
            mediaInfo["height"_ls] = castInfo->imageSize.height();

            // TODO: Images in certain formats (e.g. WebP) will be erroneously marked as animated, even if they are static.
            mediaInfo["animated"_ls] = QMovie::supportedFormats().contains(mimeType.preferredSuffix().toUtf8());

            QVariantMap tempInfo;
            auto thumbnailInfo = getMediaInfoFromTumbnail(room, castInfo->thumbnail, eventId);
            if (thumbnailInfo["source"_ls].toUrl().scheme() == "mxc"_ls) {
                tempInfo = thumbnailInfo;
            } else {
                QString blurhash = castInfo->originalInfoJson["xyz.amorgan.blurhash"_ls].toString();
                if (blurhash.isEmpty()) {
                    tempInfo["source"_ls] = QUrl();
                } else {
                    tempInfo["source"_ls] = QUrl("image://blurhash/"_ls + blurhash);
                }
            }
            mediaInfo["tempInfo"_ls] = tempInfo;
        }
    }
    if (mimeType.name().contains(QStringLiteral("video"))) {
        if (auto castInfo = static_cast<const EventContent::VideoContent *>(fileContent)) {
            mediaInfo["width"_ls] = castInfo->imageSize.width();
            mediaInfo["height"_ls] = castInfo->imageSize.height();
            mediaInfo["duration"_ls] = castInfo->duration;

            if (!isThumbnail) {
                QVariantMap tempInfo;
                auto thumbnailInfo = getMediaInfoFromTumbnail(room, castInfo->thumbnail, eventId);
                if (thumbnailInfo["source"_ls].toUrl().scheme() == "mxc"_ls) {
                    tempInfo = thumbnailInfo;
                } else {
                    QString blurhash = castInfo->originalInfoJson["xyz.amorgan.blurhash"_ls].toString();
                    if (blurhash.isEmpty()) {
                        tempInfo["source"_ls] = QUrl();
                    } else {
                        tempInfo["source"_ls] = QUrl("image://blurhash/"_ls + blurhash);
                    }
                }
                mediaInfo["tempInfo"_ls] = tempInfo;
            }
        }
    }
    if (mimeType.name().contains(QStringLiteral("audio"))) {
        if (auto castInfo = static_cast<const EventContent::AudioContent *>(fileContent)) {
            mediaInfo["duration"_ls] = castInfo->duration;
        }
    }

    return mediaInfo;
}

QVariantMap EventHandler::getMediaInfoFromTumbnail(const NeoChatRoom *room, const Quotient::EventContent::Thumbnail &thumbnail, const QString &eventId)
{
    QVariantMap thumbnailInfo;

    if (!thumbnail.url().isValid() || thumbnail.url().scheme() != QStringLiteral("mxc") || eventId.isEmpty()) {
        thumbnailInfo["source"_ls] = QUrl();
    } else {
        QUrl source = room->makeMediaUrl(eventId, thumbnail.url());

        if (source.isValid()) {
            thumbnailInfo["source"_ls] = source;
        } else {
            thumbnailInfo["source"_ls] = QUrl();
        }
    }

    auto mimeType = thumbnail.mimeType;
    // Add the MIME type for the media if available.
    thumbnailInfo["mimeType"_ls] = mimeType.name();

    // Add the MIME type icon if available.
    thumbnailInfo["mimeIcon"_ls] = mimeType.iconName();

    // Add media size if available.
    thumbnailInfo["size"_ls] = thumbnail.payloadSize;

    thumbnailInfo["width"_ls] = thumbnail.imageSize.width();
    thumbnailInfo["height"_ls] = thumbnail.imageSize.height();

    return thumbnailInfo;
}

bool EventHandler::hasReply(const Quotient::RoomEvent *event, bool showFallbacks)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "hasReply called with event set to nullptr.";
        return false;
    }

    const auto relations = event->contentPart<QJsonObject>("m.relates_to"_ls);
    if (!relations.isEmpty()) {
        const bool hasReplyRelation = relations.contains("m.in_reply_to"_ls);
        bool isFallingBack = relations["is_falling_back"_ls].toBool();
        return hasReplyRelation && (showFallbacks ? true : !isFallingBack);
    }
    return false;
}

QString EventHandler::replyId(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "replyId called with event set to nullptr.";
        return {};
    }
    return event->contentJson()["m.relates_to"_ls].toObject()["m.in_reply_to"_ls].toObject()["event_id"_ls].toString();
}

Quotient::RoomMember EventHandler::replyAuthor(const NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (room == nullptr) {
        qCWarning(EventHandling) << "replyAuthor called with room set to nullptr.";
        return {};
    }
    if (event == nullptr) {
        qCWarning(EventHandling) << "replyAuthor called with event set to nullptr. Returning empty user.";
        return {};
    }

    if (auto replyPtr = room->getReplyForEvent(*event)) {
        return room->member(replyPtr->senderId());
    } else {
        return room->member(QString());
    }
}

bool EventHandler::isThreaded(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "isThreaded called with event set to nullptr.";
        return false;
    }

    return (event->contentPart<QJsonObject>("m.relates_to"_ls).contains("rel_type"_ls)
            && event->contentPart<QJsonObject>("m.relates_to"_ls)["rel_type"_ls].toString() == "m.thread"_ls)
        || (!event->unsignedPart<QJsonObject>("m.relations"_ls).isEmpty() && event->unsignedPart<QJsonObject>("m.relations"_ls).contains("m.thread"_ls));
}

QString EventHandler::threadRoot(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "threadRoot called with event set to nullptr.";
        return {};
    }

    // Get the thread root ID from m.relates_to if it exists.
    if (event->contentPart<QJsonObject>("m.relates_to"_ls).contains("rel_type"_ls)
        && event->contentPart<QJsonObject>("m.relates_to"_ls)["rel_type"_ls].toString() == "m.thread"_ls) {
        return event->contentPart<QJsonObject>("m.relates_to"_ls)["event_id"_ls].toString();
    }
    // For thread root events they have an m.relations in the unsigned part with a m.thread object.
    // If so return the event ID as it is the root.
    if (!event->unsignedPart<QJsonObject>("m.relations"_ls).isEmpty() && event->unsignedPart<QJsonObject>("m.relations"_ls).contains("m.thread"_ls)) {
        return id(event);
    }
    return {};
}

float EventHandler::latitude(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "latitude called with event set to nullptr.";
        return -100.0;
    }

    const auto geoUri = event->contentJson()["geo_uri"_ls].toString();
    if (geoUri.isEmpty()) {
        return -100.0; // latitude runs from -90deg to +90deg so -100 is out of range.
    }
    const auto latitude = geoUri.split(u';')[0].split(u':')[1].split(u',')[0];
    return latitude.toFloat();
}

float EventHandler::longitude(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "longitude called with event set to nullptr.";
        return -200.0;
    }

    const auto geoUri = event->contentJson()["geo_uri"_ls].toString();
    if (geoUri.isEmpty()) {
        return -200.0; // longitude runs from -180deg to +180deg so -200 is out of range.
    }
    const auto latitude = geoUri.split(u';')[0].split(u':')[1].split(u',')[1];
    return latitude.toFloat();
}

QString EventHandler::locationAssetType(const Quotient::RoomEvent *event)
{
    if (event == nullptr) {
        qCWarning(EventHandling) << "locationAssetType called with event set to nullptr.";
        return {};
    }

    const auto assetType = event->contentJson()["org.matrix.msc3488.asset"_ls].toObject()["type"_ls].toString();
    if (assetType.isEmpty()) {
        return {};
    }
    return assetType;
}

#include "moc_eventhandler.cpp"
