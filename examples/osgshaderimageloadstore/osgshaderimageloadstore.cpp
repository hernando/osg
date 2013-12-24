#include <assert.h>
#include <iostream>

#include <osg/GL>
#include <osg/GLExtensions>

#include <osg/ArgumentParser>
#include <osg/Geode>
#include <osg/ImageTextureBinding>
#include <osg/TextureRectangle>
#include <osgViewer/Viewer>

// This callback initializes an 1-channel integer texture with 0's
class SubloadCallback : public osg::TextureRectangle::SubloadCallback
{
public:
    virtual void load(const osg::TextureRectangle &t, osg::State &s) const
    {
        size_t height = t.getTextureHeight();
        size_t width = t.getTextureWidth();
        std::vector<unsigned int> data(height * width * 4, 0);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, t.getInternalFormat(),
                     width, height, 0, GL_RGBA_INTEGER,
                     GL_UNSIGNED_INT, &data[0]);
    }

    virtual void subload(const osg::TextureRectangle &t, osg::State &s) const
    {}
};

class DrawCallback : public osg::Drawable::DrawCallback
{
public:
    void drawImplementation(osg::RenderInfo &renderInfo,
                            const osg::Drawable *drawable) const
    {
        initExtension();

        drawable->drawImplementation(renderInfo);
        // Wait until all writes are finished
        glMemoryBarrier(GL_ALL_BARRIER_BITS_EXT);
    }

    void initExtension() const
    {
        if (!glMemoryBarrier)
        {
            if (!osg::setGLExtensionFuncPtr(glMemoryBarrier, "glMemoryBarrier",
                                            "glMemoryBarrierEXT"))
            {
                std::cerr << "Unsupported extension "
                          << "EXT_shader_image_load_store" << std::endl;
                abort();
            }
        }
    }

    static void (*glMemoryBarrier)(GLbitfield);
};

void (*DrawCallback::glMemoryBarrier)(GLbitfield) = 0;


void readPixels(osg::State &state, osg::TextureRectangle *texture)
{
    GLint previous;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previous);

    // Texture readback. This is not the most orthodox way, but it is compact
    osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject();
    osg::FrameBufferAttachment buffer(texture);
    fbo->setAttachment(osg::Camera::COLOR_BUFFER0, buffer);
    fbo->apply(state);
    osg::ref_ptr<osg::Image> image = new osg::Image();
    image->readPixels(
        0, 0, texture->getTextureWidth(), texture->getTextureHeight(),
        GL_RED_INTEGER, GL_UNSIGNED_INT);

    // Printing the 16x16 values of the texture
    unsigned int *data = (unsigned int*)image->data();
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            std::cout << data[i * 16 + j] << ' ';
        }
        std::cout << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, previous);
}

int main(int argc, char *argv[])
{
    osg::ArgumentParser args(&argc, argv);

    osgViewer::Viewer viewer(args);

    // Creating the geometry object with the quads
    osg::Geometry *geometry = new osg::Geometry();
    osg::Vec3Array *vertices = new osg::Vec3Array();
    vertices->push_back(osg::Vec3(-1, -1, 0));
    vertices->push_back(osg::Vec3(1, -1, 0));
    vertices->push_back(osg::Vec3(1, 1, 0));
    vertices->push_back(osg::Vec3(-1, 1, 0));
    osg::DrawElementsUInt *indices =
        new osg::DrawElementsUInt(GL_QUADS);
    for (int i = 0; i < 100; ++i) {
        indices->push_back(0);
        indices->push_back(1);
        indices->push_back(2);
        indices->push_back(3);
    }
    geometry->setVertexArray(vertices);
    geometry->addPrimitiveSet(indices);
    osg::Geode *scene = new osg::Geode();
    scene->addDrawable(geometry);

    // Vertex and fragment shaders
    osg::StateSet *stateSet = scene->getOrCreateStateSet();
    osg::Program *program = new osg::Program();
    osg::Shader *vert = new osg::Shader(osg::Shader::VERTEX);
    vert->setShaderSource(
        "#version 410\n"

        "in vec3 osg_Vertex;"

        "void main()"
        "{"
        "    gl_Position = vec4(osg_Vertex, 1);"
        "}");
    osg::Shader *frag = new osg::Shader(osg::Shader::FRAGMENT);
    std::string source =
        "#version 410\n"
        "#extension GL_ARB_shader_bit_encoding : enable\n"
        "#extension GL_EXT_shader_image_load_store : enable\n"
        "#extension GL_EXT_gpu_shader4 : enable\n"
        "#extension GL_ARB_texture_rectangle : enable\n"
        "out vec4 color;";
    // Adding the layout declaration if early tests (z, alpha, ...) are
    // enabled.
    if (args.read("--early-tests"))
        source +=
            "layout(early_fragment_tests) in;";
    source +=
        "layout(size1x32) uniform uimage2DRect buffer;"

        "void main()"
        "{"
        "    imageAtomicAdd(buffer, ivec2(gl_FragCoord.xy), 1);"
        "    color = vec4(2);"
        "}";
    frag->setShaderSource(source);
    program->addShader(vert);
    program->addShader(frag);

    stateSet->setAttributeAndModes(program);

    int window_size = 16;
    viewer.setUpViewInWindow(0, 0, window_size, window_size);
    viewer.setSceneData(scene);

    // Creating the target texture
    osg::TextureRectangle *buffer;
    buffer = new osg::TextureRectangle();
    buffer->setTextureSize(window_size, window_size);
    buffer->setInternalFormat(GL_R32UI);
    buffer->setSourceFormat(GL_RED_INTEGER);
    buffer->setSubloadCallback(new SubloadCallback());
    osg::ImageTextureBinding *binding =
        new osg::ImageTextureBinding(0, buffer,
                                     osg::ImageTextureBinding::WRITE_ONLY);
    geometry->getOrCreateStateSet()->setAttributeAndModes(binding);
    geometry->getOrCreateStateSet()->setTextureAttribute(0, buffer);

    // The draw callback is reponsible of calling glMemoryBarrierEXT
    geometry->setDrawCallback(new DrawCallback());

    viewer.realize();
    // We use single threaded made to make sure that the no other thread is
    // using GL after viewer.frame() returns.
    viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
    // Rendering a single frame
    viewer.frame();

    // Reading back the texture and printing the contents
    osgViewer::ViewerBase::Contexts contexts;
    viewer.getContexts(contexts);
    contexts.front()->makeCurrent();
    readPixels(*contexts.front()->getState(), buffer);
}

