/******************************************************************************
 * Copyright (C) 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "roomdialogs.h"

#include "quaternionroom.h"
#include <user.h>
#include <connection.h>
#include <csapi/create_room.h>
#include <logging.h>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QCompleter>
#include <QtGui/QStandardItemModel>

#include <QtCore/QElapsedTimer>

RoomDialogBase::RoomDialogBase(const QString& title,
        const QString& applyButtonText,
        QuaternionRoom* r, QWidget* parent, const connections_t& cs,
        QDialogButtonBox::StandardButtons extraButtons)
    : Dialog(title, parent, StatusLine, applyButtonText, extraButtons)
    , connections(cs), room(r), avatar(new QLabel)
    , account(r ? nullptr : new QComboBox)
    , roomName(new QLineEdit)
    , aliasServer(new QLabel), alias(new QLineEdit)
    , topic(new QPlainTextEdit)
    , publishRoom(new QCheckBox(tr("Publish room in room directory")))
    , guestCanJoin(new QCheckBox(tr("Allow guest accounts to join the room")))
    , formLayout(addLayout<QFormLayout>())
{
    if (room)
    {
        avatar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        avatar->setPixmap({64, 64});
    }
    topic->setTabChangesFocus(true);
    topic->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);

    // Layout controls

    if (!room)
        formLayout->addRow(new QLabel(
                tr("Please fill the fields as desired. None are mandatory")));
    {
        if (room)
        {
            auto* topLayout = new QHBoxLayout;
            topLayout->addWidget(avatar);
            {
                auto* topFormLayout = new QFormLayout;
                if (connections.size() > 1)
                    topFormLayout->addRow(tr("Account"), account);
                topFormLayout->addRow(tr("Room name"), roomName);
                topFormLayout->addRow(tr("Primary alias"), alias);
                topLayout->addLayout(topFormLayout);
            }
            formLayout->addRow(topLayout);
        } else {
            if (connections.size() > 1)
                formLayout->addRow(tr("Account"), account);
            formLayout->addRow(tr("Room name"), roomName);
            auto* aliasLayout = new QHBoxLayout;
            aliasLayout->addWidget(new QLabel("#"));
            aliasLayout->addWidget(alias);
            aliasLayout->addWidget(aliasServer);
            formLayout->addRow(tr("Primary alias"), aliasLayout);
        }
    }
    formLayout->addRow(tr("Topic"), topic);
    if (!room) // TODO: Support this in RoomSettingsDialog as well
    {
        formLayout->addRow(publishRoom);
//        formLayout->addRow(guestCanJoin); // TODO: QMatrixClient/libqmatrixclient#36

    }

    if (connections.size() > 1)
        account->setFocus();
    else
        roomName->setFocus();
}

RoomSettingsDialog::RoomSettingsDialog(QuaternionRoom* room, QWidget* parent)
    : RoomDialogBase(tr("Room settings: %1").arg(room->displayName()),
                     tr("Update room"), room, parent)
    , tagsList(new QListWidget)
{
    Q_ASSERT(room != nullptr);
    connect(room, &QuaternionRoom::avatarChanged, this, [this, room] {
        if (!userChangedAvatar)
            avatar->setPixmap(QPixmap::fromImage(room->avatar(64)));
    });
    avatar->setPixmap(QPixmap::fromImage(room->avatar(64)));
    tagsList->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);
    tagsList->setUniformItemSizes(true);
    tagsList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    formLayout->addRow(tr("Tags"), tagsList);
}

void RoomSettingsDialog::load()
{
    roomName->setText(room->name());
    alias->setText(room->canonicalAlias());
    topic->setPlainText(room->topic());
    // QPlainTextEdit may change some characters before any editing occurs;
    // so save this already adjusted topic to compare later.
    previousTopic = topic->toPlainText();

    tagsList->clear();
    auto roomTags = room->tagNames();
    for (const auto& tag: room->connection()->tagNames())
    {
        auto tagDisplayName =
                tag == QMatrixClient::FavouriteTag ? tr("Favourites") :
                tag == QMatrixClient::LowPriorityTag ? tr("Low priority") :
                tag.startsWith("u.") ? tag.mid(2) :
                tag;
        auto* item = new QListWidgetItem(tagDisplayName, tagsList);
        item->setData(Qt::UserRole, tag);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);
        item->setCheckState(
                    roomTags.contains(tag) ? Qt::Checked : Qt::Unchecked);
        item->setToolTip(tag);
        tagsList->addItem(item);
    }
}

void RoomSettingsDialog::apply()
{
    if (roomName->text() != room->name())
        room->setName(roomName->text());
    if (alias->text() != room->canonicalAlias())
        room->setCanonicalAlias(alias->text());
    if (topic->toPlainText() != previousTopic)
        room->setTopic(topic->toPlainText());
    auto tags = room->tags();
    for (int i = 0; i < tagsList->count(); ++i)
    {
        const auto* item = tagsList->item(i);
        const auto tagName = item->data(Qt::UserRole).toString();
        if (item->checkState() == Qt::Checked)
            tags[tagName]; // Just ensure the tag is there, no overwriting
        else
            tags.remove(tagName);
    }
    room->setTags(tags);
    accept();
}

class NextInvitee : public QComboBox
{
    public:
        using QComboBox::QComboBox;

    private:
        void focusInEvent(QFocusEvent* event) override
        {
            QComboBox::focusInEvent(event);
            static_cast<CreateRoomDialog*>(parent())->updatePushButtons();
        }
        void focusOutEvent(QFocusEvent* event) override
        {
            QComboBox::focusOutEvent(event);
            static_cast<CreateRoomDialog*>(parent())->updatePushButtons();
        }
};

class InviteeList : public QListWidget
{
    public:
        using QListWidget::QListWidget;

    private:
        void keyPressEvent(QKeyEvent* event) override
        {
            if (event->key() ==  Qt::Key_Delete)
                delete takeItem(currentRow());
        }
        void mousePressEvent(QMouseEvent* event) override
        {
            if (event->button() == Qt::MidButton)
                delete takeItem(currentRow());
        }
};

CreateRoomDialog::CreateRoomDialog(const connections_t& connections,
                                   QWidget* parent)
    : RoomDialogBase(tr("Create room"), tr("Create room"),
                     nullptr, parent, connections, NoExtraButtons)
    , nextInvitee(new NextInvitee)
    , inviteButton(new QPushButton(tr("Add", "Add a user to the list of invitees")))
    , invitees(new QListWidget)
{
    Q_ASSERT(!connections.isEmpty());

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    connect(account, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreateRoomDialog::accountSwitched);
#else
    connect(account, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &CreateRoomDialog::accountSwitched);
#endif

    nextInvitee->setEditable(true);
    nextInvitee->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    nextInvitee->setMinimumContentsLength(42);
    auto* completer = new QCompleter(nextInvitee);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    nextInvitee->setCompleter(completer);
    connect(nextInvitee, &NextInvitee::currentTextChanged,
            this, &CreateRoomDialog::updatePushButtons);
//    connect(nextInvitee, &NextInvitee::editTextChanged,
//            this, &CreateRoomDialog::updateUserList);
    inviteButton->setFocusPolicy(Qt::NoFocus);
    inviteButton->setDisabled(true);
    connect(inviteButton, &QPushButton::clicked, [this] {
        auto userName = nextInvitee->currentText();
        if (userName.indexOf('@') == -1)
        {
            userName.prepend('@');
            if (userName.indexOf(':') == -1)
            {
                auto* conn = account->currentData(Qt::UserRole)
                                        .value<QMatrixClient::Connection*>();
                userName += ':' + conn->homeserver().authority();
            }
        }
        auto* item = new QListWidgetItem(userName);
        if (nextInvitee->currentIndex() != -1)
            item->setData(Qt::UserRole, nextInvitee->currentData(Qt::UserRole));
        invitees->addItem(item);
        nextInvitee->clear();
    });
    invitees->setSizeAdjustPolicy(
                QAbstractScrollArea::AdjustToContentsOnFirstShow);
    invitees->setUniformItemSizes(true);
    invitees->setSortingEnabled(true);

    // Layout additional controls

    auto* inviteLayout = new QHBoxLayout;
    inviteLayout->addWidget(nextInvitee);
    inviteLayout->addWidget(inviteButton);

    formLayout->addRow(tr("Invite user(s)"), inviteLayout);
    formLayout->addRow("", invitees);

    setPendingApplyMessage(tr("Creating the room, please wait"));
}

void CreateRoomDialog::updatePushButtons()
{
    inviteButton->setEnabled(!nextInvitee->currentText().isEmpty());
    if (inviteButton->isEnabled() && nextInvitee->hasFocus())
        inviteButton->setDefault(true);
    else
        buttonBox()->button(QDialogButtonBox::Ok)->setDefault(true);
}

void CreateRoomDialog::load()
{
    qDebug() << "Loading the dialog";
    account->clear();
    for (auto* c: connections)
        account->addItem(c->userId(), QVariant::fromValue(c));
    roomName->clear();
    alias->clear();
    topic->clear();
    previousTopic.clear();
    nextInvitee->clear();
    accountSwitched();
    invitees->clear();
}

void CreateRoomDialog::apply()
{
    using namespace QMatrixClient;
    auto* connection = account->currentData(Qt::UserRole).value<Connection*>();
    QStringList userIds;
    for (int i = 0; i < invitees->count(); ++i)
    {
        auto userVar = invitees->item(i)->data(Qt::UserRole);
        if (auto* user = userVar.value<User*>())
            userIds.push_back(user->id());
        else
            userIds.push_back(invitees->item(i)->text());
    }
    auto* job = connection->createRoom(
            publishRoom->isChecked() ?
                Connection::PublishRoom : Connection::UnpublishRoom,
            alias->text(), roomName->text(), topic->toPlainText(),
            userIds, "", {}, false);

    connect(job, &BaseJob::success, this, &Dialog::accept);
    connect(job, &BaseJob::failure, this, [this,job] {
        applyFailed(job->errorString());
    });
}

void CreateRoomDialog::accountSwitched()
{
    auto* connection = account->currentData(Qt::UserRole)
                                    .value<QMatrixClient::Connection*>();
    aliasServer->setText(':' + connection->homeserver().authority());

    auto* completer = nextInvitee->completer();
    Q_ASSERT(completer != nullptr);
    auto* model = new QStandardItemModel(completer);

    auto savedCurrentText = nextInvitee->currentText();
//    auto prefix =
//            savedCurrentText.midRef(savedCurrentText.startsWith('@') ? 1 : 0);
//    if (prefix.size() >= 3)
//    {
        Q_ASSERT(connection != nullptr);
        QElapsedTimer et; et.start();
        for (auto* u: connection->users())
        {
            if (!u->isGuest())
            {
                auto* item = new QStandardItem(u->fullName());
                item->setData(QVariant::fromValue(u));
                model->appendRow(item);
            }
        }
        qDebug() << "Completion candidates:" << model->rowCount()
                 << "out of" << connection->users().size()
                 << "filtered in" << et;
//    }
    nextInvitee->setModel(model);
    nextInvitee->setEditText(savedCurrentText);
    completer->setCompletionPrefix(savedCurrentText);
}
