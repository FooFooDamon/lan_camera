/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Main GUI class of this client program.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __QLANCAMERA_HPP__
#define __QLANCAMERA_HPP__

#include <memory>

#include "ui_QLANCamera.h"

struct biz_context;
class QSoundEffect;
class QMediaPlayer;

class QLANCamera : public QDialog, public Ui_Dialog
{
    Q_OBJECT

public:
    QLANCamera() = delete;
    QLANCamera(struct biz_context *ctx, QWidget *parent = nullptr);
    Q_DISABLE_COPY_MOVE(QLANCamera);
    ~QLANCamera();

public:
    void infoBox(const QString &title, const QString &text);
    void warningBox(const QString &title, const QString &text);
    void errorBox(const QString &title, const QString &text);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

signals:
    void delegatingResize(int width, int height);
    void delegatingClose(void);
    void delegatingPlayRecordStartAudio(void);
    void delegatingPlayRecordEndAudio(void);
    void delegatingAddFileToVideoList(const char *path);
    void delegatingRemoveFileFromVideoList(const char *path);
    void delegatingRemoveDirFromVideoList(const char *path);
    void delegatingSyncLocalVideos(void);
    void delegatingReloadVideoList(void);

private slots:
    void __delegatingResize(int width, int height);
    void __delegatingClose(void);
    void __delegatingPlayRecordStartAudio(void);
    void __delegatingPlayRecordEndAudio(void);
    void switchVideoTab(void);
    void updateProgressPosition(qint64 position);
    void updateProgressDuration(qint64 duration);
    void addFileToVideoList(const char *path);
    void removeFileFromVideoList(const char *path);
    void removeDirFromVideoList(const char *path);
    void syncLocalVideosIfNeeded(void);
    void reloadVideoList(void);
    void switchVideoListItemStatus(QTreeWidgetItem *item, int column);
    void switchVideoDirIcon(QTreeWidgetItem *item);
    void on_tab_currentChanged(int index);

private:
    struct biz_context *m_ctx;
    std::string m_window_title;
    bool m_found_delegating_close;
    std::shared_ptr<QSoundEffect> m_start_sound;
    std::shared_ptr<QSoundEffect> m_end_sound;
    std::shared_ptr<QMediaPlayer> m_video_player;
    std::string m_video_root_dir;
};

#endif /* #ifndef __QLANCAMERA_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

