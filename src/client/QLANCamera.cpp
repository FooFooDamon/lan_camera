// SPDX-License-Identifier: Apache-2.0

/*
 * Main GUI class of this client program.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <arpa/inet.h>

#include "versions.h" // For FULL_VERSION()

#include "QLANCamera.hpp"

#include <stack>
#include <jsoncpp/json/version.h>
#include <opencv2/core/version.hpp>
//#include <QTextCodec> // QT += core
#include <QDir> // QT += core
#include <QCloseEvent> // QT += gui
#include <QMessageBox> // QT += widgets
#include <QScrollBar> // QT += widgets
#include <QSoundEffect> // QT += multimedia
#include <QMediaPlayer> // QT += multimedia
#include <QVideoWidget> // QT += multimediawidgets

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"

//const QTextCodec *G_TEXT_CODEC = QTextCodec::codecForName("UTF8"/*"GB2312"*/);

static void load_record_hint_audio(std::shared_ptr<QSoundEffect> &player, const char *path, float volume)
{
    if (player)
    {
        LOG_WARNING("** Audio already loaded: %s", path);

        return;
    }

    player = std::make_shared<QSoundEffect>();
    player->setSource(QUrl::fromLocalFile(path));
    player->setVolume(volume);
    LOG_DEBUG("%s: status is %d, volume is %.02f", path, player->status(), volume);
}

QLANCamera::QLANCamera(struct biz_context *ctx, QWidget *parent/* = nullptr*/)
    : QDialog(parent)
    , m_ctx(ctx)
    , m_found_delegating_close(false)
{
    const auto &conf = *ctx->conf;
    const auto &size = conf.player.canvas_sizes[conf.player.which_size - 1];

    setupUi(this);

    load_record_hint_audio(m_start_sound, conf.audio.record_start.c_str(), conf.audio.volume);
    load_record_hint_audio(m_end_sound, conf.audio.record_end.c_str(), conf.audio.volume);

    this->initCameraTab();
    this->initVideosTab();
    this->initCanvasTab();
    this->initAboutTab();

    this->connectSignalsAndSlots();

    this->delegatingReloadVideoList();
    this->delegatingResize(size.first, size.second);
    this->setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    this->setWindowTitle(QString::asprintf("%s [v%s]", this->windowTitle().toStdString().c_str(), FULL_VERSION()));
    m_window_title = this->windowTitle().toStdString();
    this->lblRealtimeInfo->setAlignment(Qt::AlignTop);
#if 0 // not working
    this->sldProgressBar->setWindowFlags(Qt::WindowStaysOnTopHint);
    this->sldProgressBar->show();
#else // working
    //this->sldProgressBar->raise(); // But the width of bar can't be adjusted any more. Why?!
#endif
}

QLANCamera::~QLANCamera()
{
    // empty
}

#define SHOW_MSG_BOX(_type, _title, _text)                      do { \
    bool visible = this->isVisible(); \
    QMessageBox::_type(visible ? this : this->parentWidget(), visible ? _title : this->windowTitle(), _text); \
} while (0)

void QLANCamera::infoBox(const QString &title, const QString &text)
{
    SHOW_MSG_BOX(information, title, text);
}

void QLANCamera::warningBox(const QString &title, const QString &text)
{
    SHOW_MSG_BOX(warning, title, text);
}

void QLANCamera::errorBox(const QString &title, const QString &text)
{
    SHOW_MSG_BOX(critical, title, text);
}

void QLANCamera::closeEvent(QCloseEvent *event)/* override */
{
    if (m_found_delegating_close)
        event->accept();
    else
    {
        QMessageBox::StandardButton button = QMessageBox::question(
            this, "", "Exit now?", QMessageBox::Yes | QMessageBox::No);

        if (QMessageBox::Yes == button)
            event->accept();
        else
            event->ignore();
    }
}

void QLANCamera::keyPressEvent(QKeyEvent *event)/* override */
{
    if (event->key() == Qt::Key_Escape)
        event->ignore();
    else
        QWidget::keyPressEvent(event);
}

static void sync_local_videos_if_needed(const biz_context_t &ctx)
{
    auto &cmd_args = *ctx.cmd_args;
    auto &conf = *ctx.conf;

    if (0 == inet_addr(conf.save.sync.ip.c_str()))
        return;

    static thread_local std::string s_sync_cmd;

    if (s_sync_cmd.empty())
    {
        const auto &sync = conf.save.sync;
        std::string file_prefix("file://");
        std::string::size_type prefix_pos = sync.password.find(file_prefix);
        bool uses_passwd_file = (std::string::npos != prefix_pos);
        std::string passwd_env = uses_passwd_file ? "" : (std::string(" RSYNC_PASSWORD='") + sync.password + "'");
        std::string passwd_file = uses_passwd_file ? sync.password.substr(prefix_pos + file_prefix.size()) : "";
        const struct
        {
            const char *name;
            const std::string &value;
        } fields_to_check[] = {
            { "user", sync.user },
            { "ip", sync.ip },
            { "root directory", conf.save.dir },
        };

        for (const auto &f : fields_to_check)
        {
            if (std::string::npos != f.value.find('\'') || std::string::npos != f.value.find('"'))
            {
                LOG_ERROR("*** Found single/double quotes in field[%s]: %s", f.name, f.value.c_str());

                return;
            }
        }

        if (std::string::npos != sync.password.find('\'') || std::string::npos != sync.password.find('"'))
        {
            LOG_ERROR("*** Found single/double quotes in password, which is not safe!");

            return;
        }

        s_sync_cmd = "bash -c \"(time"; // Set the type of Shell to bash to avoid awful output of time command.
        s_sync_cmd += passwd_env;
        s_sync_cmd += " rsync -arvz --delete --perms --links --hard-links --times -P";
        s_sync_cmd += uses_passwd_file ? (std::string(" --password-file='") + passwd_file + "'") : "";
        s_sync_cmd += QString::asprintf(" 'rsync://%s@%s:%d/%s' '%s/server'",
            sync.user.c_str(), sync.ip.c_str(), sync.port, "lan_camera", conf.save.dir.c_str()).toStdString();
        s_sync_cmd += ")\" 2>&1";
        if (uses_passwd_file)
            LOG_NOTICE("%s", s_sync_cmd.c_str());
    } // if (s_sync_cmd.empty())

    FILE *stream = popen(s_sync_cmd.c_str(), "r");

    if (nullptr == stream)
    {
        LOG_ERROR("*** popen() failed: %s", strerror(errno));

        return;
    }

    const std::string &log_level = ("config" == cmd_args.log_level) ? conf.logger.level : cmd_args.log_level;
    bool is_debug = ("debug" == log_level);
    char buf[1024] = {};

    if (is_debug)
        LOG_DEBUG("Synchronizing server video files to local ...");

    while (nullptr != fgets(buf, sizeof(buf), stream))
    {
        if (is_debug)
            fprintf(stderr, "%s", buf);
    }

    pclose(stream);
}

void QLANCamera::keyReleaseEvent(QKeyEvent *event)/* override */
{
    int key = event->key();

    switch (key)
    {
    case Qt::Key_Escape:
        this->close();
        break;

    case Qt::Key_Left:
    case Qt::Key_Right:
        if (this->tab->currentIndex() == this->tab->indexOf(this->tabPlayer))
        {
            auto &player = this->m_video_player;
            QSlider *progbar = this->sldProgressBar;
            int leap = progbar->singleStep();
            int cur_pos = progbar->value();
            int max_pos = progbar->maximum();

            if (Qt::Key_Left == key)
                player->setPosition((leap <= cur_pos) ? (cur_pos - leap) : 0);
            else
                player->setPosition((cur_pos + leap <= max_pos) ? (cur_pos + leap) : max_pos);

            //this->vidwdtCanvas->setFocus();
        }
        else
            QWidget::keyReleaseEvent(event);
        break;

    case Qt::Key_F5:
        if (this->tab->currentIndex() == this->tab->indexOf(this->tabVideos))
        {
            this->treewdtFiles->hide(); // FIXME: This does not work, neither does setVisible(false)! Why?
            sync_local_videos_if_needed(*m_ctx);
            this->reloadVideoList();
            this->treewdtFiles->show();
            this->infoBox("Refresh", "Finished synchronizing server video files to local.");
        }
        else
            QWidget::keyReleaseEvent(event);
        break;

    case Qt::Key_Space:
        if (ROLE_CLIENT == m_ctx->conf->role.type && this->tab->currentIndex() == this->tab->indexOf(this->tabCamera))
        {
            m_ctx->needs_live_stream = !m_ctx->needs_live_stream;
            //this->infoBox("Streaming",
            //    QString::asprintf("Turned %s live stream.", m_ctx->needs_live_stream ? "ON" : "OFF"));
        }
        else
            QWidget::keyReleaseEvent(event);
        break;

    default:
        QWidget::keyReleaseEvent(event);
        break;
    }
}

void QLANCamera::initCameraTab(void)
{
    ; // empty
}

void QLANCamera::initVideosTab(void)
{
    const auto &conf = *m_ctx->conf;

    if ("player" == m_ctx->cmd_args->biz)
        m_video_root_dir = m_ctx->cmd_args->play_dir;
    else
        m_video_root_dir = conf.save.dir + ((ROLE_CLIENT == conf.role.type && conf.save.enabled) ? "/client" : "/server");
    //this->treewdtFiles->setStyleSheet("background: black;");
    this->treewdtFiles->verticalScrollBar()->setStyleSheet("background: rgba(128, 128, 128, 128);");
    //this->tabVideos->setEnabled(true); // FIXME: What is this for?!
}

void QLANCamera::initCanvasTab(void)
{
    m_video_player = std::make_shared<QMediaPlayer>(this);
    m_video_player->setVideoOutput(this->vidwdtCanvas);
    this->vidwdtCanvas->setPlayer(m_video_player.get());
    this->sldProgressBar->setPlayer(m_video_player.get());
#if 1
    this->tab->setTabVisible(this->tab->indexOf(this->tabPlayer), false);
#else // This does not work as expetected.
    this->tabPlayer->setVisible(false);
#endif
}

void QLANCamera::initAboutTab(void)
{
    this->tabAbout->setAutoFillBackground(true); // The palette settings would not take effect without this.
    // The hyperlink is considered a local path without this. It's okay to set it in *.ui file.
    //this->txtDeclaration->setOpenExternalLinks(true);
    for (std::pair<QTextBrowser *, const char *> item : {
        std::pair<QTextBrowser *, const char *>(this->txtQt, QT_VERSION_STR),
        std::pair<QTextBrowser *, const char *>(this->txtOpenCV, CV_VERSION),
        std::pair<QTextBrowser *, const char *>(this->txtJsonCpp, JSONCPP_VERSION_STRING),
    })
    {
        QTextCursor cursor = item.first->textCursor();

        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QString(" ") + item.second);
        //item.first->setOpenExternalLinks(true);
    }
}

#define SET_TEXTBROWSER_UNSELECTABLE(_browser_)      \
    this->connect(_browser_, &QTextBrowser::copyAvailable, this, [this](bool yes) { \
        if (yes) \
        { \
            QTextCursor cursor = _browser_->textCursor(); \
\
            cursor.clearSelection(); \
            _browser_->setTextCursor(cursor); \
        } \
    })

void QLANCamera::connectSignalsAndSlots(void)
{
    this->connect(this, SIGNAL(delegatingResize(int,int)),
        this, SLOT(__delegatingResize(int,int)));
    this->connect(this, SIGNAL(delegatingClose(void)),
        this, SLOT(__delegatingClose(void)));

    this->connect(this, SIGNAL(delegatingPlayRecordStartAudio(void)),
        this, SLOT(__delegatingPlayRecordStartAudio(void)));
    this->connect(this, SIGNAL(delegatingPlayRecordEndAudio(void)),
        this, SLOT(__delegatingPlayRecordEndAudio(void)));

    this->connect(this, SIGNAL(delegatingAddFileToVideoList(const char *)),
        this, SLOT(addFileToVideoList(const char *)));
    this->connect(this, SIGNAL(delegatingRemoveFileFromVideoList(const char *)),
        this, SLOT(removeFileFromVideoList(const char *)));
    this->connect(this, SIGNAL(delegatingRemoveDirFromVideoList(const char *)),
        this, SLOT(removeDirFromVideoList(const char *)));
    this->connect(this, SIGNAL(delegatingSyncLocalVideos(void)),
        this, SLOT(syncLocalVideosIfNeeded(void)));
    this->connect(this, SIGNAL(delegatingReloadVideoList(void)),
        this, SLOT(reloadVideoList(void)));
    this->connect(this->treewdtFiles, SIGNAL(itemActivated(QTreeWidgetItem *, int)),
        this, SLOT(switchVideoListItemStatus(QTreeWidgetItem *, int)));
    this->connect(this->treewdtFiles, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
        this, SLOT(switchVideoListItemStatus(QTreeWidgetItem *, int)));
    this->connect(this->treewdtFiles, SIGNAL(itemCollapsed(QTreeWidgetItem *)),
        this, SLOT(switchVideoDirIcon(QTreeWidgetItem *)));
    this->connect(this->treewdtFiles, SIGNAL(itemExpanded(QTreeWidgetItem *)),
        this, SLOT(switchVideoDirIcon(QTreeWidgetItem *)));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    this->connect(m_video_player.get(), SIGNAL(stateChanged(QMediaPlayer::State)),
#else
    this->connect(m_video_player.get(), SIGNAL(playbackStateChanged(QMediaPlayer::PlaybackState)),
#endif
        this, SLOT(switchVideoTab(void)));
    this->connect(m_video_player.get(), SIGNAL(positionChanged(qint64)),
        this, SLOT(updateProgressPosition(qint64)));
    this->connect(m_video_player.get(), SIGNAL(durationChanged(qint64)),
        this, SLOT(updateProgressDuration(qint64)));

    SET_TEXTBROWSER_UNSELECTABLE(this->txtQt);
    SET_TEXTBROWSER_UNSELECTABLE(this->txtOpenCV);
    SET_TEXTBROWSER_UNSELECTABLE(this->txtJsonCpp);
    SET_TEXTBROWSER_UNSELECTABLE(this->txtThanks);
}

void QLANCamera::__delegatingResize(int width, int height)
{
    int orig_widget_height = this->height();
    int orig_tab_height = this->tab->height();
    int orig_canvas_height = this->glwdtCanvas->height();
    QLabel *progtext = this->lblPlayProgress;
    QSlider *progbar = this->sldProgressBar;
    int progbar_height = progbar->height();

    this->glwdtCanvas->setFixedSize(width, height);
    this->treewdtFiles->setFixedSize(width, height);
    this->vidwdtCanvas->setFixedSize(width, height - progbar_height);
    this->tab->setFixedSize(width, height + (orig_tab_height - orig_canvas_height));
    this->setFixedSize(width, height + (orig_widget_height - orig_canvas_height));
    progtext->setGeometry(width - progtext->width(), height - progtext->height(), progtext->width(), progtext->height());
    progbar->setGeometry(progbar->x(), height - progbar_height, width - progtext->width(), progbar_height);
}

void QLANCamera::__delegatingClose()
{
    m_found_delegating_close = true;
    this->close();
}

void QLANCamera::__delegatingPlayRecordStartAudio(void)
{
    if (m_start_sound)
        m_start_sound->play();
    else
        LOG_ERROR("*** Audio not ready yet! Please call loadRecordStartAudio() to load it first!");
}

void QLANCamera::__delegatingPlayRecordEndAudio(void)
{
    if (m_end_sound)
        m_end_sound->play();
    else
        LOG_ERROR("*** Audio not ready yet! Please call loadRecordEndAudio() to load it first!");
}

void QLANCamera::switchVideoTab(void)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool stopped = (QMediaPlayer::StoppedState == m_video_player->state());
#else
    bool stopped = (QMediaPlayer::StoppedState == m_video_player->playbackState());
#endif

    this->tab->setTabVisible(this->tab->indexOf(this->tabPlayer), !stopped);
    this->tab->setTabVisible(this->tab->indexOf(this->tabVideos), stopped);
    this->tab->setCurrentWidget(stopped ? this->tabVideos : this->tabPlayer);
    this->setWindowTitle(stopped ? QString::fromStdString(m_window_title)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        : m_video_player->media().request().url().path().remove(0, m_video_root_dir.size() + 1));
#else
        : m_video_player->source().path().remove(0, m_video_root_dir.size() + 1));
#endif
}

static inline std::tuple<int, int, int> convert_milliseconds(qint64 msecs)
{
    int seconds = msecs / 1000;
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;

    return { hours, minutes, seconds % 60 };
}

void QLANCamera::updateProgressPosition(qint64 pos)
{
    auto position = convert_milliseconds(pos);
    auto duration = convert_milliseconds(this->m_video_player->duration());

    this->lblPlayProgress->setText(
        QString::asprintf("%02d:%02d:%02d/%02d:%02d:%02d",
            std::get<0>(position), std::get<1>(position), std::get<2>(position),
            std::get<0>(duration), std::get<1>(duration), std::get<2>(duration))
    );
}

void QLANCamera::updateProgressDuration(qint64 duration)
{
    this->sldProgressBar->setRange(0, duration);
    if (duration < 10 * 1000) // < 10s
    {
        this->sldProgressBar->setSingleStep(duration / 10);
        this->sldProgressBar->setPageStep(duration / 5);
    }
    else if (duration >= 10 * 1000 && duration < 20 * 1000) // [10s, 20s)
    {
        this->sldProgressBar->setSingleStep(2 * 1000);
        this->sldProgressBar->setPageStep(4 * 1000);
    }
    else if (duration >= 20 * 1000 && duration < 40 * 1000) // [20s, 40s)
    {
        this->sldProgressBar->setSingleStep(4 * 1000);
        this->sldProgressBar->setPageStep(8 * 1000);
    }
    else if (duration >= 40 * 1000 && duration < 80 * 1000) // [40s, 80s)
    {
        this->sldProgressBar->setSingleStep(8 * 1000);
        this->sldProgressBar->setPageStep(16 * 1000);
    }
    else if (duration >= 80 * 1000 && duration < 160 * 1000) // [80s, 160s)
    {
        this->sldProgressBar->setSingleStep(8 * 1000);
        this->sldProgressBar->setPageStep(32 * 1000);
    }
    else // >= 160s
    {
        this->sldProgressBar->setSingleStep(8 * 1000);
        this->sldProgressBar->setPageStep(64 * 1000);
    }
}

void QLANCamera::addFileToVideoList(const char *path)
{
    this->reloadVideoList();
}

void QLANCamera::removeFileFromVideoList(const char *path)
{
    LOG_DEBUG("TODO: %s", path);
}

void QLANCamera::removeDirFromVideoList(const char *path)
{
    LOG_DEBUG("TODO: %s", path);
}

static std::vector<QString> get_selected_hierarchies(const QTreeWidget *widget)
{
    QList<QTreeWidgetItem *> selected_items = widget->selectedItems(); // Actually, it could have one at most.
    QTreeWidgetItem *item = selected_items.empty() ? nullptr : selected_items[0];
    std::vector<QString> result;

    if (!item)
        return result;

    std::stack<QString> stack;
    const int MAX_HIERARCHIES = 5;
    int i = 0;

    do
    {
        stack.push(item->text(0));
        item = item->parent();
        if (nullptr == item || (QTreeWidgetItem *)widget == item)
            break;
    }
    while (++i < MAX_HIERARCHIES);

    result.reserve(stack.size());
    while (!stack.empty())
    {
        result.push_back(std::move(stack.top()));
        stack.pop();
    }

    return result;
}

static inline QTreeWidgetItem* find_child_by_name(const QTreeWidgetItem *parent, const QString &name)
{
    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem *child = parent->child(i);

        if (name == child->text(0))
            return child;
    }

    return nullptr;
}

// https://specifications.freedesktop.org/icon-naming-spec/latest/
// https://specifications.freedesktop.org/icon-theme-spec/latest/
static const thread_local auto S_ICON_VIDEO = QIcon::fromTheme("video-x-generic");
static const thread_local auto S_ICON_FOLDER_CLOSE = QIcon::fromTheme("folder");
static const thread_local auto S_ICON_FOLDER_OPEN = QIcon::fromTheme("folder-open");

static inline void set_tree_item(QTreeWidgetItem *item, const QString &text, const QIcon &icon)
{
    static thread_local auto s_font = QFont(item->font(0).family(), item->font(0).pointSize() * 3 / 2);

    item->setText(0, text);
    item->setFont(0, s_font);
    item->setForeground(0, QColor(255, 255, 255));
    item->setIcon(0, icon);
}

void QLANCamera::syncLocalVideosIfNeeded(void)
{
    this->treewdtFiles->hide(); // FIXME: This does not work, neither does setVisible(false)! Why?
    sync_local_videos_if_needed(*m_ctx);
    this->treewdtFiles->show();
}

void QLANCamera::reloadVideoList(void)
{
    const char *root_dir = m_video_root_dir.c_str();
    QDir root(root_dir);

    if (!root.exists())
    {
        this->errorBox("Directory Error", QString::asprintf("Non-existent or unreadable directory:\n\n%s", root_dir));

        return;
    }

    auto dir_filters = QDir::Filter::Dirs | QDir::Filter::Readable | QDir::Filter::NoDotAndDotDot;
    auto file_filters = QDir::Filter::Files | QDir::Filter::Readable | QDir::Filter::NoDotAndDotDot;
    auto sort_flags = QDir::SortFlag::Name;
    auto *video_list = this->treewdtFiles;
    std::vector<QString> selected_hierarchies = get_selected_hierarchies(video_list);
    QTreeWidgetItem *year_item = nullptr;
    QTreeWidgetItem *month_item = nullptr;
    QTreeWidgetItem *day_item = nullptr;
    QTreeWidgetItem *hour_item = nullptr;
    QTreeWidgetItem *video_item = nullptr;

    video_list->clear();
    for (const auto &year : root.entryList(dir_filters, sort_flags))
    {
        QDir year_dir(root.path() + "/" + year);

        video_list->addTopLevelItem(new QTreeWidgetItem(video_list));
        year_item = video_list->topLevelItem(video_list->topLevelItemCount() - 1);
        set_tree_item(year_item, year, S_ICON_FOLDER_CLOSE);
        for (const auto &month : year_dir.entryList(dir_filters, sort_flags))
        {
            QDir month_dir(year_dir.path() + "/" + month);

            year_item->addChild(new QTreeWidgetItem(year_item));
            month_item = year_item->child(year_item->childCount() - 1);
            set_tree_item(month_item, month, S_ICON_FOLDER_CLOSE);
            for (const auto &day : month_dir.entryList(dir_filters, sort_flags))
            {
                QDir day_dir(month_dir.path() + "/" + day);

                month_item->addChild(new QTreeWidgetItem(month_item));
                day_item = month_item->child(month_item->childCount() - 1);
                set_tree_item(day_item, day, S_ICON_FOLDER_CLOSE);
                for (const auto &hour : day_dir.entryList(dir_filters, sort_flags))
                {
                    QDir hour_dir(day_dir.path() + "/" + hour);

                    day_item->addChild(new QTreeWidgetItem(day_item));
                    hour_item = day_item->child(day_item->childCount() - 1);
                    set_tree_item(hour_item, hour, S_ICON_FOLDER_CLOSE);
                    for (const auto &video : hour_dir.entryList(file_filters, sort_flags))
                    {
                        hour_item->addChild(new QTreeWidgetItem(hour_item));
                        video_item = hour_item->child(hour_item->childCount() - 1);
                        set_tree_item(video_item, video, S_ICON_VIDEO);
                    } // for (video : hour_dir)
                } // for (hour : day_dir)
            } // for (day : month_dir)
        } // for (month : year_dir)
    } // for (year : root)

    if (video_list->topLevelItemCount() <= 0)
        return;

    if (selected_hierarchies.size() > 0)
    {
        for (int i = 0; i < video_list->topLevelItemCount(); ++i)
        {
            year_item = video_list->topLevelItem(i);
            if (selected_hierarchies[0] == year_item->text(0))
                break;
            else
                year_item = nullptr;
        }
    }

    month_item = (year_item && selected_hierarchies.size() > 1)
        ? find_child_by_name(year_item, selected_hierarchies[1]) : nullptr;

    day_item = (month_item && selected_hierarchies.size() > 2)
        ? find_child_by_name(month_item, selected_hierarchies[2]) : nullptr;

    hour_item = (day_item && selected_hierarchies.size() > 3)
        ? find_child_by_name(day_item, selected_hierarchies[3]) : nullptr;

    video_item = (hour_item && selected_hierarchies.size() > 4)
        ? find_child_by_name(hour_item, selected_hierarchies[4]) : nullptr;

    QTreeWidgetItem *cur_item = nullptr;
    QTreeWidgetItem *item_priorities[] = {
        video_item, hour_item, day_item, month_item, year_item,
        video_list->topLevelItem(video_list->topLevelItemCount() - 1)
    };

    for (auto &item : item_priorities)
    {
        if (item)
        {
            cur_item = item;
            video_list->expandItem(cur_item);

            break;
        }
    }

    video_list->setCurrentItem(cur_item);
}

void QLANCamera::switchVideoListItemStatus(QTreeWidgetItem *item, int column)
{
    if (m_video_root_dir.empty() || nullptr == item || column < 0)
        return;

    if (!item->text(column).endsWith(".mp4"))
    {
        if (item->isExpanded())
            this->treewdtFiles->collapseItem(item);
        else
            this->treewdtFiles->expandItem(item);

        return;
    }

    QTreeWidgetItem *hour_item = item->parent();
    QTreeWidgetItem *day_item = hour_item->parent();
    QTreeWidgetItem *month_item = day_item->parent();
    QTreeWidgetItem *year_item = month_item->parent();
    QString video_path = QString::fromStdString(m_video_root_dir) + "/" + year_item->text(column)
        + "/" + month_item->text(column) + "/" + day_item->text(column) + "/" + hour_item->text(column)
        + "/" + item->text(column);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_video_player->setMedia(QUrl::fromLocalFile(video_path));
#else
    m_video_player->setSource(QUrl::fromLocalFile(video_path));
#endif
    m_video_player->play();
    this->tab->setCurrentWidget(this->tabPlayer);
}

void QLANCamera::switchVideoDirIcon(QTreeWidgetItem *item)
{
    if (item && !item->text(0).endsWith(".mp4"))
        item->setIcon(0, item->isExpanded() ? S_ICON_FOLDER_OPEN : S_ICON_FOLDER_CLOSE);
}

void QLANCamera::on_tab_currentChanged(int index)
{
    if (this->tab->indexOf(this->tabPlayer) == index)
        this->vidwdtCanvas->setFocus();
    else if (this->tab->indexOf(this->tabVideos) == index)
        this->treewdtFiles->setFocus();
    else
    {
        // empty
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 *
 * >>> 2026-04-22, Man Hung-Coeng <udc577@126.com>:
 *  01. Add initialization for the new About tab, and refactor the constructor.
 */

