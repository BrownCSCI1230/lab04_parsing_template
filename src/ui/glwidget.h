#ifndef GLWIDGET_H
#define GLWIDGET_H

#include "parser/sceneparser.h"
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

class GLWidget : public QOpenGLWidget
{
public:
    GLWidget(QWidget *parent) : QOpenGLWidget(parent) { }
    ~GLWidget();

    void loadScene(const RenderData &renderData);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QOpenGLShaderProgram m_program;

    QOpenGLVertexArrayObject m_vaoCone;
    QOpenGLVertexArrayObject m_vaoCube;
    QOpenGLVertexArrayObject m_vaoCylinder;
    QOpenGLVertexArrayObject m_vaoSphere;

    QOpenGLBuffer m_vboCone;
    QOpenGLBuffer m_vboCube;
    QOpenGLBuffer m_vboCylinder;
    QOpenGLBuffer m_vboSphere;

    glm::mat4x4 m_view;
    glm::mat4x4 m_proj;
    float m_fovy;
    QVector3D m_lightPos;

    RenderData m_renderData;
};

#endif // GLWIDGET_H
