#include <cassert>
#include <iostream>

#include <osg/GLExtensions>
#include <osg/ImageTextureBinding>
#include <osg/State>
#include <osg/Texture>

using namespace osg;

ImageTextureBinding::ImageTextureBinding()
{}

ImageTextureBinding::~ImageTextureBinding()
{}

ImageTextureBinding::ImageTextureBinding(unsigned int index, 
                                         osg::Texture *texture,
                                         Access access, int level, 
                                         bool layered, int layer) :
    _index(index),
    _texture(texture),
    _format(_texture->getInternalFormat()),
    _level(level),
    _layered(layered ? GL_TRUE : GL_FALSE),
    _layer(layer),
    _access(access)
{

}

ImageTextureBinding::ImageTextureBinding(const ImageTextureBinding &itb, 
                                         const osg::CopyOp &op) :
    _index(itb._index),
    _texture(itb._texture),
    _format(_texture->getInternalFormat()),
    _level(itb._level),
    _layered(itb._layered),
    _layer(itb._layer),
    _access(itb._access)
{}

int ImageTextureBinding::compare(const osg::StateAttribute& itb) const
{
    COMPARE_StateAttribute_Types(ImageTextureBinding, itb)

    COMPARE_StateAttribute_Parameter(_index)
    COMPARE_StateAttribute_Parameter(_texture)
    COMPARE_StateAttribute_Parameter(_format)
    COMPARE_StateAttribute_Parameter(_level)
    COMPARE_StateAttribute_Parameter(_layered)
    COMPARE_StateAttribute_Parameter(_layer)
    COMPARE_StateAttribute_Parameter(_access)
    return 0;
}

void ImageTextureBinding::apply(osg::State &state) const
{
    static void (*glBindImageTexture)(GLuint, GLuint, GLint, GLboolean,
                                         GLint, GLenum, GLint) = 0;
    if (glBindImageTexture == 0)
    {
        osg::setGLExtensionFuncPtr(glBindImageTexture, "glBindImageTexture",
                                   "glBindImageTextureEXT");
        if (glBindImageTexture == 0) {
            std::cerr << "Unsupported extension GL_EXT_shader_image_load_store"
                      << std::endl;
            abort();
        }
    }

    if (_texture.get() != 0)
    {
        unsigned int contextID = state.getContextID();
        _texture->apply(state);
        osg::Texture::TextureObject *to =
            _texture->getTextureObject(contextID);
        assert(to != 0);
        glBindImageTexture(_index, to->id(), _level, _layered, _layer,
                           (GLenum) _access, _format);
        if (state.getGlobalDefaultAttribute(getType(), _index)) {
            ImageTextureBinding *default_itb = new ImageTextureBinding();
            default_itb->_index = _index;
            state.setGlobalDefaultAttribute(default_itb);
        }
    } else {
        /* Any format is OK as long as it is a valid enum value. */
        glBindImageTexture(_index, 0, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_RGBA32F);
    }
}

void ImageTextureBinding::setTexture(osg::Texture *texture)
{
    _texture = texture;
    _format = texture->getInternalFormat();
}
