/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Canvas class based on OpenGL for displaying camera frames.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __QOPENGLCANVAS_HPP__
#define __QOPENGLCANVAS_HPP__

#include <QOpenGLWidget> // QT += widgets
#include <QOpenGLFunctions> // QT += gui
//#include <QOpenGLFunctions_3_2_Core> // QT += gui

struct biz_context;
class QOpenGLTexture;
class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QImage;
class QMenu;

class QOpenGLCanvas : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    QOpenGLCanvas() = delete;
    QOpenGLCanvas(QWidget *parent = nullptr);
    Q_DISABLE_COPY_MOVE(QOpenGLCanvas);
    ~QOpenGLCanvas();

public:
    int prepare(const unsigned char **buf_array, int buf_count, struct biz_context &ctx);
    int prepare(const std::vector<unsigned char *> &buffers, struct biz_context &ctx);
    int prepare(const std::vector<std::vector<unsigned char>> &buffers, struct biz_context &ctx);

protected:
    void initializeGL(void) override;
    void paintGL(void) override;
    void resizeGL(int width, int height) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

signals:
    void render(int buf_index);

private slots:
    void __render(int buf_index);

private:
    struct biz_context *m_ctx;
    std::shared_ptr<QMenu> m_context_menu;
    std::shared_ptr<QAction> m_stream_on_action;
    std::shared_ptr<QAction> m_stream_off_action;
    std::string m_gl_version;
    std::shared_ptr<QOpenGLTexture> m_texture;
#ifdef DISABLE_OPENGL_SHADERS
    std::vector<std::shared_ptr<QImage>> m_images;
#else
    std::shared_ptr<QOpenGLShaderProgram> m_program;
    std::shared_ptr<QOpenGLBuffer> m_vbo; // Vertex Buffer Object
#endif
    std::vector<const unsigned char *> m_buffers;
    int m_buf_index;
    int m_width;
    int m_height;
};

#endif /* #ifndef __QOPENGLCANVAS_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

