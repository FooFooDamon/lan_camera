// SPDX-License-Identifier: Apache-2.0

/*
 * Canvas class based on OpenGL for displaying camera frames.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include "QOpenGLCanvas.hpp"

#include <QOpenGLBuffer> // QT += gui
#include <QOpenGLTexture> // QT += gui
#include <QOpenGLShaderProgram> // QT += gui
#include <QImage> // QT += gui
#include <QMenu> // QT += widgets

#include "fmt_log.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"

QOpenGLCanvas::QOpenGLCanvas(QWidget *parent/* = nullptr*/)
    : QOpenGLWidget(parent)
    , m_ctx(nullptr)
    , m_buf_index(-1)
    , m_width(0)
    , m_height(0)
{
    m_gl_version = "320 es"; // can be updated by prepare() afterwards
    this->connect(this, SIGNAL(render(int)), this, SLOT(__render(int)));
}

QOpenGLCanvas::~QOpenGLCanvas()
{
    if (m_texture)
        m_texture->destroy();

#ifndef DISABLE_OPENGL_SHADERS
    if (m_program)
    {
        //m_program->disableAttributeArray("positionCoord");
        //m_program->disableAttributeArray("textureCoord");
        m_program->removeAllShaders();
    }

    if (m_vbo)
        m_vbo->destroy();
#endif
}

int QOpenGLCanvas::prepare(const unsigned char **buf_array, int buf_count, struct biz_context &ctx)
{
    if (nullptr == buf_array || buf_count <= 0)
    {
        LOG_ERROR("*** Null pointer or invalid count: buf_array = %p, buf_count = %d", buf_array, buf_count);

        return (nullptr == buf_array) ? -EFAULT : -EINVAL;
    }

    std::vector<unsigned char *> buffers(buf_count);

    for (int i = 0; i < buf_count; ++i)
    {
        buffers[i] = const_cast<unsigned char *>(buf_array[i]);
    }

    return this->prepare(buffers, ctx);
}

int QOpenGLCanvas::prepare(const std::vector<unsigned char *> &buffers, struct biz_context &ctx)
{
    if (buffers.empty())
    {
        LOG_ERROR("*** Empty buffer holder!");

        return -EINVAL;
    }

    const auto &conf = *ctx.conf;
    const std::pair<uint16_t, uint16_t> &size = conf.camera.image_sizes[conf.camera.which_size - 1];
    int width = size.first;
    int height = size.second;

    if (width <= 0 || height <= 0)
    {
        LOG_ERROR("*** Invalid size: width = %d, height = %d", width, height);

        return -EINVAL;
    }

    m_ctx = &ctx;
    m_buffers.reserve(buffers.size());
#ifdef DISABLE_OPENGL_SHADERS
    m_images.reserve(buffers.size());
#endif
    for (int i = 0; i < (int)buffers.size(); ++i)
    {
        if (nullptr == buffers[i])
        {
            LOG_ERROR("*** buffers[%d] is null", i);
            std::vector<const unsigned char *>().swap(m_buffers);
#ifdef DISABLE_OPENGL_SHADERS
            std::vector<std::shared_ptr<QImage>>().swap(m_images);
#endif

            return -EFAULT;
        }

        m_buffers.push_back(buffers[i]);
        LOG_DEBUG("Set buffers[%d] to: %p", i, m_buffers.back());

#ifdef DISABLE_OPENGL_SHADERS
        m_images.push_back(std::make_shared<QImage>(buffers[i], width, height, QImage::Format_RGB888));
#endif
    }
    m_texture = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
#ifndef DISABLE_OPENGL_SHADERS
    m_program = std::make_shared<QOpenGLShaderProgram>();
    m_vbo = std::make_shared<QOpenGLBuffer>();
#endif

    m_width = width;
    m_height = height;
    LOG_NOTICE("Set size to: %dx%d", m_width, m_height);

    m_gl_version = conf.player.opengl.version;
    LOG_NOTICE("Set OpenGL version to: %s", m_gl_version.c_str());

    if (!m_context_menu)
    {
        m_context_menu = std::make_shared<QMenu>();

        m_stream_on_action = std::make_shared<QAction>(QIcon::fromTheme("media-playback-start"),
            "Turn on live stream\tSpace", this);
        m_context_menu->addAction(m_stream_on_action.get());
        m_stream_on_action->setVisible(!ctx.needs_live_stream);

        m_stream_off_action = std::make_shared<QAction>(QIcon::fromTheme("media-playback-stop"),
            "Turn off live stream\tSpace", this);
        m_context_menu->addAction(m_stream_off_action.get());
        m_stream_off_action->setVisible(ctx.needs_live_stream);
    }

    return 0;
}

int QOpenGLCanvas::prepare(const std::vector<std::vector<unsigned char>> &buffers, struct biz_context &ctx)
{
    if (buffers.empty())
    {
        LOG_ERROR("*** Empty buffer holder!");

        return -EINVAL;
    }

    const auto &conf = *ctx.conf;
    const std::pair<uint16_t, uint16_t> &size = conf.camera.image_sizes[conf.camera.which_size - 1];
    int width = size.first;
    int height = size.second;

    if (width <= 0 || height <= 0)
    {
        LOG_ERROR("*** Invalid size: width = %d, height = %d", width, height);

        return -EINVAL;
    }

    int min_capacity = width * height * 3;
    std::vector<unsigned char *> buf_pointers(buffers.size());

    for (int i = 0; i < (int)buffers.size(); ++i)
    {
        int length = buffers[i].size();

        if (length < min_capacity)
        {
            LOG_ERROR("*** Size of buffers[%d] is %d which is less than width x height x 3 = %d x %d x 3 = %d",
                i, length, width, height, min_capacity);

            return -EINVAL;
        }

        buf_pointers[i] = const_cast<unsigned char *>(buffers[i].data());
    }

    return this->prepare(buf_pointers, ctx);
}

static void print_opengl_info(void)
{
    GLint major, minor;

    LOG_NOTICE("GL Vendor: %s", glGetString(GL_VENDOR));
    LOG_NOTICE("GL Renderer: %s", glGetString(GL_RENDERER));
    LOG_NOTICE("GL Version(string): %s", glGetString(GL_VERSION));
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    LOG_NOTICE("GL Version(integer): %d.%d", major, minor);
    LOG_NOTICE("GL Shading Language Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    LOG_NOTICE("isOpenGLES: %s", QOpenGLContext::currentContext()->isOpenGLES() ? "true" : "false");
}

#ifndef DISABLE_OPENGL_SHADERS

static const char *S_VERTEX_SHADER_SRC = "\
#version %1\n\
in vec4 positionCoord; \n\
in vec2 textureCoord; \n\
out vec2 vTextureCoord; \n\
\n\
void main(void) \n\
{ \n\
    gl_Position = positionCoord; \n\
    vTextureCoord = textureCoord; \n\
} \n\
"; // S_VERTEX_SHADER_SRC end

static const char *S_FRAGMENT_SHADER_SRC = "\
#version %1\n\
precision mediump float; \n\
in vec2 vTextureCoord; \n\
uniform sampler2D sampler; \n\
out vec4 outFragColor; \n\
\n\
void main(void) \n\
{ \n\
    outFragColor = texture(sampler, vTextureCoord); \n\
} \n\
"; // S_FRAGMENT_SHADER_SRC end
// NOTE: Use this if OpenGL ES does not support GL_BGR:
//      outFragColor = vec4(texture(sampler, vTextureCoord).bgr, 1.0);

#endif // #ifndef DISABLE_OPENGL_SHADERS

void QOpenGLCanvas::initializeGL(void)/* override */
{
    initializeOpenGLFunctions();
    print_opengl_info();
    glClearColor(0, 0, 0, 1.0f);
    glEnable(GL_TEXTURE_2D);

#ifndef DISABLE_OPENGL_SHADERS

    GLfloat coordinates[] = {
        /* Vertex points, axis range: [-1, 1] */
        -1, 1,
        1, 1,
        1, -1,
        -1, -1,

        /* Texture points, axis range: [0, 1] */
        0, 0,
        1, 0,
        1, 1,
        0, 1,
    };
    QString vertex_shader_src(QString(S_VERTEX_SHADER_SRC).arg(m_gl_version.c_str()));
    QString frag_shader_src(QString(S_FRAGMENT_SHADER_SRC).arg(m_gl_version.c_str()));

    m_vbo->create();
    m_vbo->bind(); // NOTE: needed by allocate() below
    //m_vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo->allocate(coordinates, sizeof(coordinates));

    m_texture->create();
    m_texture->bind();
    LOG_DEBUG("texture id: %d", m_texture->textureId());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    LOG_DEBUG("Adjusted S_VERTEX_SHADER_SRC:\n----\n%s----", vertex_shader_src.toStdString().c_str());
    m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_src.replace('\n', '\r'));
    LOG_DEBUG("Adjusted S_FRAGMENT_SHADER_SRC:\n----\n%s----", frag_shader_src.toStdString().c_str());
    m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, frag_shader_src.replace('\n', '\r'));
    if (!m_program->link())
        LOG_ERROR("%s", m_program->log().toStdString().c_str());
    m_program->bind();
    m_program->setAttributeBuffer("positionCoord", GL_FLOAT, 0, 2, sizeof(GLfloat) * 2);
    m_program->enableAttributeArray("positionCoord");
    m_program->setAttributeBuffer("textureCoord", GL_FLOAT, sizeof(GLfloat) * 2 * 4, 2, sizeof(GLfloat) * 2);
    m_program->enableAttributeArray("textureCoord");
    // NOTE: The 2nd argument is a 0-based texture unit/position, not the ID of currently bound texture!
    m_program->setUniformValue("sampler", 0);
    //m_program->release(); // unbind

    if (!m_buffers.empty() && m_width > 0 && m_height > 0) // must perform a full load first
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_BGR, GL_UNSIGNED_BYTE, m_buffers[0]);

#endif // #ifndef DISABLE_OPENGL_SHADERS
}

void QOpenGLCanvas::paintGL(void)/* override */
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glDisable(GL_DEPTH_TEST);

    if (m_buffers.empty() || m_width <= 0 || m_height <= 0)
    {
        LOG_ERROR("*** Widget might not be prepared yet, call prepare() before any other step!");
        LOG_ERROR("Inner members: m_buffers.size = %lu, m_width = %d, m_height = %d",
            m_buffers.size(), m_width, m_height);

        return;
    }

    if (m_buf_index < 0)
    {
        LOG_WARNING("No available frame yet ...");

        return;
    }

    auto &texture = m_texture;

#ifdef DISABLE_OPENGL_SHADERS
    texture->setData(*m_images[m_buf_index], QOpenGLTexture::DontGenerateMipMaps); // will call create() if needed
    texture->bind();
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glEnd();
    //texture->release(); // unbind
    texture->destroy(); // destroy the previously created underlying texture object
#else
    //m_vbo->bind();

    texture->bind();
#if 0 // full load mode
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_BGR, GL_UNSIGNED_BYTE, m_buffers[m_buf_index]);
#else // update mode, usually much faster
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_BGR, GL_UNSIGNED_BYTE, m_buffers[m_buf_index]);
#endif
    //texture->setLevelofDetailBias(-1.0);

    //m_program->bind();
    glDrawArrays(GL_QUADS, 0, 4);
    //m_program->release(); // unbind

    texture->release();
    //m_vbo->release();
#endif // #ifdef DISABLE_OPENGL_SHADERS
}

void QOpenGLCanvas::contextMenuEvent(QContextMenuEvent *event)/* override */
{
    if (!m_context_menu)
    {
        LOG_ERROR("*** Context menu not initialized yet!");

        return;
    }

    m_stream_on_action->setVisible(!m_ctx->needs_live_stream);
    m_stream_off_action->setVisible(m_ctx->needs_live_stream);

    if (nullptr == m_context_menu->exec(QCursor::pos()))
        return;

    m_ctx->needs_live_stream = !m_ctx->needs_live_stream;
    LOG_NOTICE("Turned %s live stream.", m_ctx->needs_live_stream ? "ON" : "OFF");
}

void QOpenGLCanvas::resizeGL(int width, int height)/* override */
{
    glViewport(0, 0, width, height);
    LOG_NOTICE("Resized to %dx%d", width, height);
}

void QOpenGLCanvas::__render(int buf_index)
{
    m_buf_index = buf_index;
    //LOG_DEBUG("Switched to buffers[%d]: %p", m_buf_index, m_buffers[buf_index]);
    this->update();
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

