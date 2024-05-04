// SPDX-FileCopyrightText: 2018-2019 Black Hat <bhat@encom.eu.org>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <Quotient/room.h>

#include <QCache>
#include <QObject>
#include <QQmlEngine>

#include <QCoroTask>
#include <Quotient/user.h>

#include "enums/pushrule.h"
#include "pollhandler.h"

namespace Quotient
{
class User;
}

class ChatBarCache;

/**
 * @class NeoChatRoom
 *
 * This class is designed to act as a wrapper over Quotient::Room to provide API and
 * functionality not available in Quotient::Room.
 *
 * The functions fall into two main categories:
 *  - Helper functions to make functionality easily accessible in QML.
 *  - Implement functions not yet available in Quotient::Room.
 *
 * @sa Quotient::Room
 */
class NeoChatRoom : public Quotient::Room
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    /**
     * @brief A list of users currently typing in the room.
     *
     * The list does not include the local user.
     *
     * This is different to getting a list of Quotient::User objects
     * as neither of those can provide details like the displayName or avatarMediaId
     * without the room context as these can vary from room to room. This function
     * provides the room context and puts the result as a list of QVariantMap objects.
     *
     * @return a QVariantMap for the user with the following
     * parameters:
     *  - id - User ID.
     *  - avatarMediaId - Avatar id in the context of this room.
     *  - displayName - Display name in the context of this room.
     *  - display - Name in the context of this room.
     *
     * @sa Quotient::User
     */
    Q_PROPERTY(QVariantList usersTyping READ getUsersTyping NOTIFY typingChanged)

    /**
     * @brief Convenience function to get the QDateTime of the last event.
     *
     * @sa lastEvent()
     */
    Q_PROPERTY(QDateTime lastActiveTime READ lastActiveTime NOTIFY lastActiveTimeChanged)

    /**
     * @brief Whether a file is being uploaded to the server.
     */
    Q_PROPERTY(bool hasFileUploading READ hasFileUploading WRITE setHasFileUploading NOTIFY hasFileUploadingChanged)

    /**
     * @brief Progress of a file upload as a percentage 0 - 100%.
     *
     * The value will be 0 if no file is uploading.
     *
     * @sa hasFileUploading
     */
    Q_PROPERTY(int fileUploadingProgress READ fileUploadingProgress NOTIFY fileUploadingProgressChanged)

    /**
     * @brief Whether the read marker should be shown.
     */
    Q_PROPERTY(bool readMarkerLoaded READ readMarkerLoaded NOTIFY readMarkerLoadedChanged)

    /**
     * @brief The avatar image to be used for the room.
     */
    Q_PROPERTY(QString avatarMediaId READ avatarMediaId NOTIFY avatarChanged STORED false)

    /**
     * @brief Get a user object for the other person in a direct chat.
     */
    Q_PROPERTY(Quotient::User *directChatRemoteUser READ directChatRemoteUser CONSTANT)

    /**
     * @brief The Matrix IDs of this room's parents.
     *
     * Empty if no parent space is set.
     */
    Q_PROPERTY(QList<QString> parentIds READ parentIds NOTIFY parentIdsChanged)

    /**
     * @brief The current canonical parent for the room.
     *
     * Empty if no canonical parent is set. The write method can only be used to
     * set an existing parent as canonical; If you wish to add a new parent and set
     * it as canonical use the addParent method and pass true to the canonical
     * parameter.
     *
     * Setting will fail if the user doesn't have the required privileges (see
     * canModifyParent) or if the given room ID is not a parent room.
     *
     * @sa canModifyParent, addParent
     */
    Q_PROPERTY(QString canonicalParent READ canonicalParent WRITE setCanonicalParent NOTIFY canonicalParentChanged)

    /**
     * @brief If the room is a space.
     */
    Q_PROPERTY(bool isSpace READ isSpace CONSTANT)

    /**
     * @brief The number of notifications in this room's children.
     *
     * Will always return 0 if this is not a space.
     */
    Q_PROPERTY(qsizetype childrenNotificationCount READ childrenNotificationCount NOTIFY childrenNotificationCountChanged)

    /**
     * @brief Whether this room's children have any highlight notifications.
     *
     * Will always return false if this is not a space.
     */
    Q_PROPERTY(bool childrenHaveHighlightNotifications READ childrenHaveHighlightNotifications NOTIFY childrenHaveHighlightNotificationsChanged)

    /**
     * @brief Whether the local user has an invite to the room.
     *
     * False for any other state including if the local user is a member.
     */
    Q_PROPERTY(bool isInvite READ isInvite NOTIFY isInviteChanged)

    /**
     * @brief Whether the local user can send messages in the room.
     */
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)

    /**
     * @brief The current join rule for the room as a QString.
     *
     * Possible values are [public, knock, invite, private, restricted].
     *
     * @sa https://spec.matrix.org/v1.5/client-server-api/#mroomjoin_rules
     */
    Q_PROPERTY(QString joinRule READ joinRule WRITE setJoinRule NOTIFY joinRuleChanged)

    /**
     * @brief The space IDs that members of can join this room.
     *
     * Empty if the join rule is not restricted.
     */
    Q_PROPERTY(QList<QString> restrictedIds READ restrictedIds NOTIFY joinRuleChanged)

    /**
     * @brief Get the maximum room version that the server supports.
     *
     * Only returns main integer room versions (i.e. no msc room versions).
     */
    Q_PROPERTY(int maxRoomVersion READ maxRoomVersion NOTIFY maxRoomVersionChanged)

    /**
     * @brief The rule for which messages should generate notifications for the room.
     *
     * @sa PushNotificationState::State
     */
    Q_PROPERTY(PushNotificationState::State pushNotificationState READ pushNotificationState WRITE setPushNotificationState NOTIFY pushNotificationStateChanged)

    /**
     * @brief The current history visibilty setting for the room.
     *
     * Possible values are [invited, joined, shared, world_readable].
     *
     * @sa https://spec.matrix.org/v1.5/client-server-api/#room-history-visibility
     */
    Q_PROPERTY(QString historyVisibility READ historyVisibility WRITE setHistoryVisibility NOTIFY historyVisibilityChanged)

    /**
     * @brief Set the default URL preview state for room members.
     *
     * Assumed false if the org.matrix.room.preview_urls state message has never been
     * set. Can only be set if the calling user has a high enough power level.
     */
    Q_PROPERTY(bool defaultUrlPreviewState READ defaultUrlPreviewState WRITE setDefaultUrlPreviewState NOTIFY defaultUrlPreviewStateChanged)

    /**
     * @brief Enable URL previews for the local user.
     */
    Q_PROPERTY(bool urlPreviewEnabled READ urlPreviewEnabled WRITE setUrlPreviewEnabled NOTIFY urlPreviewEnabledChanged)

    /**
     * @brief Whether the local user can encrypt the room.
     *
     * A local user can encrypt a room if they have permission to send the m.room.encryption
     * state event.
     *
     * @sa https://spec.matrix.org/v1.5/client-server-api/#mroomencryption
     */
    Q_PROPERTY(bool canEncryptRoom READ canEncryptRoom NOTIFY canEncryptRoomChanged)

    /**
     * @brief The default power level in the room for new users.
     */
    Q_PROPERTY(int defaultUserPowerLevel READ defaultUserPowerLevel WRITE setDefaultUserPowerLevel NOTIFY defaultUserPowerLevelChanged)

    /**
     * @brief The power level required to invite users to the room.
     */
    Q_PROPERTY(int invitePowerLevel READ invitePowerLevel WRITE setInvitePowerLevel NOTIFY invitePowerLevelChanged)

    /**
     * @brief The power level required to kick users from the room.
     */
    Q_PROPERTY(int kickPowerLevel READ kickPowerLevel WRITE setKickPowerLevel NOTIFY kickPowerLevelChanged)

    /**
     * @brief The power level required to ban users from the room.
     */
    Q_PROPERTY(int banPowerLevel READ banPowerLevel WRITE setBanPowerLevel NOTIFY banPowerLevelChanged)

    /**
     * @brief The power level required to delete other user messages.
     */
    Q_PROPERTY(int redactPowerLevel READ redactPowerLevel WRITE setRedactPowerLevel NOTIFY redactPowerLevelChanged)

    /**
     * @brief The default power level for state events that are not explicitly specified.
     */
    Q_PROPERTY(int statePowerLevel READ statePowerLevel WRITE setStatePowerLevel NOTIFY statePowerLevelChanged)

    /**
     * @brief The default power level for event that are not explicitly specified.
     */
    Q_PROPERTY(int defaultEventPowerLevel READ defaultEventPowerLevel WRITE setDefaultEventPowerLevel NOTIFY defaultEventPowerLevelChanged)

    /**
     * @brief The power level required to change power levels for the room.
     */
    Q_PROPERTY(int powerLevelPowerLevel READ powerLevelPowerLevel WRITE setPowerLevelPowerLevel NOTIFY powerLevelPowerLevelChanged)

    /**
     * @brief The power level required to change the room name.
     */
    Q_PROPERTY(int namePowerLevel READ namePowerLevel WRITE setNamePowerLevel NOTIFY namePowerLevelChanged)

    /**
     * @brief The power level required to change the room avatar.
     */
    Q_PROPERTY(int avatarPowerLevel READ avatarPowerLevel WRITE setAvatarPowerLevel NOTIFY avatarPowerLevelChanged)

    /**
     * @brief The power level required to change the room aliases.
     */
    Q_PROPERTY(int canonicalAliasPowerLevel READ canonicalAliasPowerLevel WRITE setCanonicalAliasPowerLevel NOTIFY canonicalAliasPowerLevelChanged)

    /**
     * @brief The power level required to change the room topic.
     */
    Q_PROPERTY(int topicPowerLevel READ topicPowerLevel WRITE setTopicPowerLevel NOTIFY topicPowerLevelChanged)

    /**
     * @brief The power level required to encrypt the room.
     */
    Q_PROPERTY(int encryptionPowerLevel READ encryptionPowerLevel WRITE setEncryptionPowerLevel NOTIFY encryptionPowerLevelChanged)

    /**
     * @brief The power level required to change the room history visibility.
     */
    Q_PROPERTY(int historyVisibilityPowerLevel READ historyVisibilityPowerLevel WRITE setHistoryVisibilityPowerLevel NOTIFY historyVisibilityPowerLevelChanged)

    /**
     * @brief The power level required to pin events in the room.
     */
    Q_PROPERTY(int pinnedEventsPowerLevel READ pinnedEventsPowerLevel WRITE setPinnedEventsPowerLevel NOTIFY pinnedEventsPowerLevelChanged)

    /**
     * @brief The power level required to upgrade the room.
     */
    Q_PROPERTY(int tombstonePowerLevel READ tombstonePowerLevel WRITE setTombstonePowerLevel NOTIFY tombstonePowerLevelChanged)

    /**
     * @brief The power level required to set the room server access control list (ACL).
     */
    Q_PROPERTY(int serverAclPowerLevel READ serverAclPowerLevel WRITE setServerAclPowerLevel NOTIFY serverAclPowerLevelChanged)

    /**
     * @brief The power level required to add children to a space.
     */
    Q_PROPERTY(int spaceChildPowerLevel READ spaceChildPowerLevel WRITE setSpaceChildPowerLevel NOTIFY spaceChildPowerLevelChanged)

    /**
     * @brief The power level required to set the room parent space.
     */
    Q_PROPERTY(int spaceParentPowerLevel READ spaceParentPowerLevel WRITE setSpaceParentPowerLevel NOTIFY spaceParentPowerLevelChanged)

    /**
     * @brief The cache for the main chat bar in the room.
     */
    Q_PROPERTY(ChatBarCache *mainCache READ mainCache CONSTANT)

    /**
     * @brief The cache for the edit chat bar in the room.
     */
    Q_PROPERTY(ChatBarCache *editCache READ editCache CONSTANT)

public:
    /**
     * @brief Define the types on inline messages that can be shown.
     */
    enum MessageType {
        Positive, /**< Positive message, typically green. */
        Info, /**< Info message, typically highlight color. */
        Error, /**< Error message, typically red. */
    };
    Q_ENUM(MessageType)

    explicit NeoChatRoom(Quotient::Connection *connection, QString roomId, Quotient::JoinState joinState = {});

    /**
     * @brief Get a user in the context of this room.
     *
     * This is different to getting a Quotient::User object
     * as neither of those can provide details like the displayName or avatarMediaId
     * without the room context as these can vary from room to room. This function
     * provides the room context and outputs the result as QVariantMap.
     *
     * Can be called with an empty QString to return an empty user, which is a useful return
     * from models to avoid undefined properties.
     *
     * @param userID the ID of the user to output.
     *
     * @return a QVariantMap for the user with the following properties:
     *  - isLocalUser - Whether the user is the local user.
     *  - id - The matrix ID of the user.
     *  - displayName - Display name in the context of this room.
     *  - avatarSource - The mxc URL for the user's avatar in the current room.
     *  - avatarMediaId - Avatar id in the context of this room.
     *  - color - Color for the user.
     *  - object - The Quotient::User object for the user.
     *
     * @sa Quotient::User
     */
    Q_INVOKABLE [[nodiscard]] QVariantMap getUser(const QString &userID) const;

    /**
     * @brief Get a user in the context of this room.
     *
     * This is different to getting a Quotient::User object
     * as neither of those can provide details like the displayName or avatarMediaId
     * without the room context as these can vary from room to room. This function
     * provides the room context and outputs the result as QVariantMap.
     *
     * Can be called with a nullptr to return an empty user, which is a useful return
     * from models to avoid undefined properties.
     *
     * @param user the user to output.
     *
     * @return a QVariantMap for the user with the following properties:
     *  - isLocalUser - Whether the user is the local user.
     *  - id - The matrix ID of the user.
     *  - displayName - Display name in the context of this room.
     *  - avatarSource - The mxc URL for the user's avatar in the current room.
     *  - avatarMediaId - Avatar id in the context of this room.
     *  - color - Color for the user.
     *  - object - The Quotient::User object for the user.
     *
     * @sa Quotient::User
     */
    Q_INVOKABLE [[nodiscard]] QVariantMap getUser(Quotient::User *user) const;

    [[nodiscard]] QVariantList getUsersTyping() const;

    [[nodiscard]] QDateTime lastActiveTime();

    /**
     * @brief Get the last interesting event.
     *
     * This function respects the user's state event setting and discards
     * other not interesting events.
     *
     * @warning This function can return an empty pointer if the room does not have
     *          any RoomMessageEvents loaded.
     */
    [[nodiscard]] const Quotient::RoomEvent *lastEvent() const;

    /**
     * @brief Convenient way to check if the last event looks like it has spoilers.
     *
     * This does a basic check to see if the message contains a data-mx-spoiler
     * attribute within the text which makes it likely that the message has a spoiler
     * section. However this is not 100% reliable as during parsing it may be
     * removed if used within an illegal tag or on a tag for which data-mx-spoiler
     * is not a valid attribute.
     *
     * @sa lastEvent()
     */
    [[nodiscard]] bool lastEventIsSpoiler() const;

    /**
     * @brief Return the notification count for the room accounting for tags and notification state.
     *
     * The following rules are observed:
     *  - Rooms tagged as low priority or mentions and keywords notification state
     *    only return the number of highlights.
     *  - Muted rooms always return 0.
     */
    int contextAwareNotificationCount() const;

    [[nodiscard]] bool hasFileUploading() const;
    void setHasFileUploading(bool value);

    [[nodiscard]] int fileUploadingProgress() const;
    void setFileUploadingProgress(int value);

    /**
     * @brief Download a file for the given event to a local file location.
     */
    Q_INVOKABLE void download(const QString &eventId, const QUrl &localFilename = {});

    /**
     * @brief Download a file for the given event as a temporary file.
     */
    Q_INVOKABLE bool downloadTempFile(const QString &eventId);

    /**
     * @brief Check if the given event is highlighted.
     *
     * An event is highlighted if it contains the local user's id but was not sent by the
     * local user.
     */
    bool isEventHighlighted(const Quotient::RoomEvent *e) const;

    /**
     * @brief Convenience function to find out if the room contains the given user.
     *
     * A room contains the user if the user can be found and their JoinState is
     * not JoinState::Leave.
     */
    Q_INVOKABLE [[nodiscard]] bool containsUser(const QString &userID) const;

    /**
     * @brief True if the given user ID is banned from the room.
     */
    Q_INVOKABLE [[nodiscard]] bool isUserBanned(const QString &user) const;

    /**
     * @brief True if the local user can send the given event type.
     */
    Q_INVOKABLE [[nodiscard]] bool canSendEvent(const QString &eventType) const;

    /**
     * @brief True if the local user can send the given state event type.
     */
    Q_INVOKABLE [[nodiscard]] bool canSendState(const QString &eventType) const;

    /**
     * @brief Send a report to the server for an event.
     *
     * @param eventId the ID of the event being reported.
     * @param reason the reason given for reporting the event.
     */
    Q_INVOKABLE void reportEvent(const QString &eventId, const QString &reason);

    Q_INVOKABLE QByteArray getEventJsonSource(const QString &eventId);

    /**
     * @brief Open the media for the given event in an appropriate external app.
     *
     * Will do nothing if the event has no media.
     */
    Q_INVOKABLE void openEventMediaExternally(const QString &eventId);

    /**
     * @brief Copy the media for the given event to the clipboard.
     *
     * Will do nothing if the event has no media.
     */
    Q_INVOKABLE void copyEventMedia(const QString &eventId);

    [[nodiscard]] bool readMarkerLoaded() const;

    [[nodiscard]] QString avatarMediaId() const;

    Quotient::User *directChatRemoteUser() const;

    /**
     * @brief Whether this room has one or more parent spaces set.
     */
    Q_INVOKABLE bool hasParent() const;

    QList<QString> parentIds() const;

    /**
     * @brief Get a list of parent space objects for this room.
     *
     * Will only return retrun spaces that are know, i.e. the user has joined and
     * a valid NeoChatRoom is available.
     *
     * @param multiLevel whether the function should recursively gather all levels
     *        of parents
     */
    Q_INVOKABLE QList<NeoChatRoom *> parentObjects(bool multiLevel = false) const;

    QString canonicalParent() const;
    void setCanonicalParent(const QString &parentId);

    /**
     * @brief Whether the local user has permission to set the given space as a parent.
     *
     * @note This follows the rules determined in the Matrix spec
     *       https://spec.matrix.org/v1.7/client-server-api/#mspaceparent-relationships
     */
    Q_INVOKABLE bool canModifyParent(const QString &parentId) const;

    /**
     * @brief Add the given room as a parent.
     *
     * Will fail if the user doesn't have the required privileges (see
     * canModifyParent()).
     *
     * @sa canModifyParent()
     */
    Q_INVOKABLE void addParent(const QString &parentId, bool canonical = false, bool setParentChild = false);

    /**
     * @brief Remove the given room as a parent.
     *
     * Will fail if the user doesn't have the required privileges (see
     * canModifyParent()).
     *
     * @sa canModifyParent()
     */
    Q_INVOKABLE void removeParent(const QString &parentId);

    [[nodiscard]] bool isSpace() const;

    qsizetype childrenNotificationCount();

    bool childrenHaveHighlightNotifications() const;

    /**
     * @brief Add the given room as a child.
     *
     * Will fail if the user doesn't have the required privileges or this room is
     * not a space.
     */
    Q_INVOKABLE void addChild(const QString &childId, bool setChildParent = false, bool canonical = false, bool suggested = false, const QString &order = {});

    /**
     * @brief Remove the given room as a child.
     *
     * Will fail if the user doesn't have the required privileges or this room is
     * not a space.
     */
    Q_INVOKABLE void removeChild(const QString &childId, bool unsetChildParent = false);

    /**
     * @brief Whether the given child is a suggested room in the space.
     */
    Q_INVOKABLE bool isSuggested(const QString &childId);

    /**
     * @brief Toggle whether the given child is a suggested room in the space.
     *
     * Will fail if the user doesn't have the required privileges, this room is
     * not a space or the given room is not a child of this space.
     */
    Q_INVOKABLE void toggleChildSuggested(const QString &childId);

    void setChildOrder(const QString &childId, const QString &order = {});

    bool isInvite() const;

    bool readOnly() const;

    Q_INVOKABLE void clearInvitationNotification();

    [[nodiscard]] QString joinRule() const;

    /**
     * @brief Set the join rule for the room.
     *
     * Will fail if the user doesn't have the required privileges.
     *
     * @param joinRule the join rule [public, knock, invite, private, restricted].
     * @param allowedSpaces only used when the join rule is restricted. This is a
     *        list of space Matrix IDs that members of can join without an invite.
     *        If the rule is restricted and this list is empty it is treated as a join
     *        rule of private instead.
     *
     * @sa https://spec.matrix.org/latest/client-server-api/#mroomjoin_rules
     */
    Q_INVOKABLE void setJoinRule(const QString &joinRule, const QList<QString> &allowedSpaces = {});

    QList<QString> restrictedIds() const;

    int maxRoomVersion() const;

    /**
     * @brief Map an alias to the room and publish.
     *
     * The alias is first mapped to the room and then published as an
     * alternate alias. Publishing the alias will fail if the user does not have
     * permission to send m.room.canonical_alias event messages.
     *
     * @note This is different to Quotient::Room::setLocalAliases() as that can only
     * get the room to publish an alias that is already mapped.
     *
     * @property alias QString in the form #new_alias:server.org
     *
     * @sa Quotient::Room::setLocalAliases()
     */
    Q_INVOKABLE void mapAlias(const QString &alias);

    /**
     * @brief Unmap an alias from the room.
     *
     * An unmapped alias is also removed as either the canonical alias or an alternate
     * alias.
     *
     * @note This is different to Quotient::Room::setLocalAliases() as that can only
     * get the room to un-publish an alias, while the mapping still exists.
     *
     * @property alias QString in the form #mapped_alias:server.org
     *
     * @sa Quotient::Room::setLocalAliases()
     */
    Q_INVOKABLE void unmapAlias(const QString &alias);

    /**
     * @brief Set the canonical alias of the room to an available mapped alias.
     *
     * If the new alias was already published as an alternate alias it will be removed
     * from that list.
     *
     * @note This is an overload of the function Quotient::Room::setCanonicalAlias().
     * This is to provide the functionality to remove the new canonical alias as a
     * published alt alias when set.
     *
     * @property newAlias QString in the form #new_alias:server.org
     *
     * @sa Quotient::Room::setCanonicalAlias()
     * */
    Q_INVOKABLE void setCanonicalAlias(const QString &newAlias);

    Q_INVOKABLE void setRoomState(const QString &type, const QString &stateKey, const QByteArray &content);

    PushNotificationState::State pushNotificationState() const;
    void setPushNotificationState(PushNotificationState::State state);

    [[nodiscard]] QString historyVisibility() const;
    void setHistoryVisibility(const QString &historyVisibilityRule);

    [[nodiscard]] bool defaultUrlPreviewState() const;
    void setDefaultUrlPreviewState(const bool &defaultUrlPreviewState);

    [[nodiscard]] bool urlPreviewEnabled() const;
    void setUrlPreviewEnabled(const bool &urlPreviewEnabled);

    bool canEncryptRoom() const;

    /**
     * @brief Get the power level for the given user ID in the room.
     *
     * Returns the default value for a user in the room if they have no escalated
     * privileges or if they are not a member so membership should be known before using.
     */
    Q_INVOKABLE [[nodiscard]] int getUserPowerLevel(const QString &userId) const;

    Q_INVOKABLE void setUserPowerLevel(const QString &userID, const int &powerLevel);

    [[nodiscard]] int powerLevel(const QString &eventName, const bool &isStateEvent = false) const;
    void setPowerLevel(const QString &eventName, const int &newPowerLevel, const bool &isStateEvent = false);

    [[nodiscard]] int defaultUserPowerLevel() const;
    void setDefaultUserPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int invitePowerLevel() const;
    void setInvitePowerLevel(const int &newPowerLevel);

    [[nodiscard]] int kickPowerLevel() const;
    void setKickPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int banPowerLevel() const;
    void setBanPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int redactPowerLevel() const;
    void setRedactPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int statePowerLevel() const;
    void setStatePowerLevel(const int &newPowerLevel);

    [[nodiscard]] int defaultEventPowerLevel() const;
    void setDefaultEventPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int powerLevelPowerLevel() const;
    void setPowerLevelPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int namePowerLevel() const;
    void setNamePowerLevel(const int &newPowerLevel);

    [[nodiscard]] int avatarPowerLevel() const;
    void setAvatarPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int canonicalAliasPowerLevel() const;
    void setCanonicalAliasPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int topicPowerLevel() const;
    void setTopicPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int encryptionPowerLevel() const;
    void setEncryptionPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int historyVisibilityPowerLevel() const;
    void setHistoryVisibilityPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int pinnedEventsPowerLevel() const;
    void setPinnedEventsPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int tombstonePowerLevel() const;
    void setTombstonePowerLevel(const int &newPowerLevel);

    [[nodiscard]] int serverAclPowerLevel() const;
    void setServerAclPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int spaceChildPowerLevel() const;
    void setSpaceChildPowerLevel(const int &newPowerLevel);

    [[nodiscard]] int spaceParentPowerLevel() const;
    void setSpaceParentPowerLevel(const int &newPowerLevel);

    ChatBarCache *mainCache() const;

    ChatBarCache *editCache() const;

    /**
     * @brief Reply to the last message sent in the timeline.
     *
     * @note This checks a maximum of the previous 35 message for performance reasons.
     */
    Q_INVOKABLE void replyLastMessage();

    /**
     * @brief Edit the last message sent by the local user.
     *
     * @note This checks a maximum of the previous 35 message for performance reasons.
     */
    Q_INVOKABLE void editLastMessage();

    /**
     * @brief Get a PollHandler object for the given event Id.
     *
     * Will return an existing PollHandler if one already exists for the event ID.
     * A new PollHandler will be created if one doesn't exist.
     *
     * @note Requires libQuotient 0.7.
     *
     * @sa PollHandler
     */
    PollHandler *poll(const QString &eventId) const;

    /**
     * @brief Create a PollHandler object for the given event.
     *
     * @sa PollHandler
     */
    void createPollHandler(const Quotient::PollStartEvent *event);

    /**
     * @brief Get the full Json data for a given room account data event.
     */
    Q_INVOKABLE QByteArray roomAcountDataJson(const QString &eventType);

    Q_INVOKABLE [[nodiscard]] QUrl avatarForMember(Quotient::User *user) const;

    /**
     * @brief Returns the event that is being replied to. This includes events that were manually loaded using NeoChatRoom::loadReply.
     */
    const Quotient::RoomEvent *getReplyForEvent(const Quotient::RoomEvent &event) const;

    /**
     * Loads the event replyId with the given id from the server and saves it locally.
     * For models to update correctly, eventId must be the event that is replying to replyId.
     * Intended to load the replied-to event when it isn't available locally.
     */
    Q_INVOKABLE void loadReply(const QString &eventId, const QString &replyId);

    /**
     * If we're invited to this room, the user that invited us. Undefined in other cases.
     */
    Q_INVOKABLE Quotient::User *invitingUser() const;

private:
    QSet<const Quotient::RoomEvent *> highlights;

    bool m_hasFileUploading = false;
    int m_fileUploadingProgress = 0;

    PushNotificationState::State m_currentPushNotificationState = PushNotificationState::Unknown;
    bool m_pushNotificationStateUpdating = false;

    void checkForHighlights(const Quotient::TimelineItem &ti);

    void onAddNewTimelineEvents(timeline_iter_t from) override;
    void onAddHistoricalTimelineEvents(rev_iter_t from) override;
    void onRedaction(const Quotient::RoomEvent &prevEvent, const Quotient::RoomEvent &after) override;

    QCoro::Task<void> doDeleteMessagesByUser(const QString &user, QString reason);
    QCoro::Task<void> doUploadFile(QUrl url, QString body = QString());

    std::unique_ptr<Quotient::RoomEvent> m_cachedEvent;

    ChatBarCache *m_mainCache;
    ChatBarCache *m_editCache;

    QCache<QString, PollHandler> m_polls;
    std::vector<Quotient::event_ptr_tt<Quotient::RoomEvent>> m_extraEvents;

private Q_SLOTS:
    void updatePushNotificationState(QString type);

    void cacheLastEvent();

Q_SIGNALS:
    void cachedInputChanged();
    void busyChanged();
    void hasFileUploadingChanged();
    void fileUploadingProgressChanged();
    void backgroundChanged();
    void readMarkerLoadedChanged();
    void parentIdsChanged();
    void canonicalParentChanged();
    void lastActiveTimeChanged();
    void childrenNotificationCountChanged();
    void childrenHaveHighlightNotificationsChanged();
    void isInviteChanged();
    void readOnlyChanged();
    void displayNameChanged();
    void pushNotificationStateChanged(PushNotificationState::State state);
    void showMessage(MessageType messageType, const QString &message);
    void canEncryptRoomChanged();
    void joinRuleChanged();
    void historyVisibilityChanged();
    void defaultUrlPreviewStateChanged();
    void urlPreviewEnabledChanged();
    void maxRoomVersionChanged();
    void defaultUserPowerLevelChanged();
    void invitePowerLevelChanged();
    void kickPowerLevelChanged();
    void banPowerLevelChanged();
    void redactPowerLevelChanged();
    void statePowerLevelChanged();
    void defaultEventPowerLevelChanged();
    void powerLevelPowerLevelChanged();
    void namePowerLevelChanged();
    void avatarPowerLevelChanged();
    void canonicalAliasPowerLevelChanged();
    void topicPowerLevelChanged();
    void encryptionPowerLevelChanged();
    void historyVisibilityPowerLevelChanged();
    void pinnedEventsPowerLevelChanged();
    void tombstonePowerLevelChanged();
    void serverAclPowerLevelChanged();
    void spaceChildPowerLevelChanged();
    void spaceParentPowerLevelChanged();
    void replyLoaded(const QString &eventId, const QString &replyId);

public Q_SLOTS:
    /**
     * @brief Upload a file to the matrix server and post the file to the room.
     *
     * @param url the location of the file to be uploaded.
     * @param body the caption that is to be given to the file.
     */
    void uploadFile(const QUrl &url, const QString &body = QString());

    /**
     * @brief Accept an invitation for the local user to join the room.
     */
    void acceptInvitation();

    /**
     * @brief Leave and forget the room for the local user.
     *
     * @note This means that not only will the user no longer receive events in
     *       the room but the will forget any history up to this point.
     *
     * @sa https://spec.matrix.org/latest/client-server-api/#leaving-rooms
     */
    void forget();

    /**
     * @brief Set the typing notification state on the room for the local user.
     */
    void sendTypingNotification(bool isTyping);

    /**
     * @brief Send a message to the room.
     *
     * @param rawText the text as it was typed.
     * @param cleanedText the text marked up as html.
     * @param type the type of message being sent.
     * @param replyEventId the id of the message being replied to if a reply.
     * @param relateToEventId the id of the message being edited if an edit.
     */
    void postMessage(const QString &rawText,
                     const QString &cleanedText,
                     Quotient::MessageEventType type = Quotient::MessageEventType::Text,
                     const QString &replyEventId = QString(),
                     const QString &relateToEventId = QString(),
                     const QString &threadRootId = QString());

    /**
     * @brief Send an html message to the room.
     *
     * @param text the text as it was typed.
     * @param html the text marked up as html.
     * @param type the type of message being sent.
     * @param replyEventId the id of the message being replied to if a reply.
     * @param relateToEventId the id of the message being edited if an edit.
     */
    void postHtmlMessage(const QString &text,
                         const QString &html,
                         Quotient::MessageEventType type = Quotient::MessageEventType::Text,
                         const QString &replyEventId = QString(),
                         const QString &relateToEventId = QString(),
                         const QString &threadRootId = QString());

    /**
     * @brief Set the room avatar.
     */
    void changeAvatar(const QUrl &localFile);

    /**
     * @brief Toggle the reaction state of the given reaction for the local user.
     */
    void toggleReaction(const QString &eventId, const QString &reaction);

    /**
     * @brief Delete recent messages by the given user.
     *
     * This will delete all messages by that user in this room that are currently loaded.
     */
    void deleteMessagesByUser(const QString &user, const QString &reason);

    /**
     *  @brief Sends a location to a room
     *  The event is sent in the migration format as specified in MSC3488
     * @param lat latitude
     * @param lon longitude
     * @param description description for the location
     */
    void sendLocation(float lat, float lon, const QString &description);
};
