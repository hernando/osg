/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/
#include <osg/ClipNode>
#include <osg/NodeCallback>
#include <osgUtil/CullVisitor>
#include <osg/GL>

#include <algorithm>

using namespace osg;

ClipNode::ClipNode():
    _value(StateAttribute::ON),
    _referenceFrame(RELATIVE_RF)
{
    setStateSet(new StateSet);
#if !(defined(OSG_GL_FIXED_FUNCTION_AVAILABLE) && !defined(OSG_GLES1_AVAILABLE))
    _setUniforms();
#endif
}

ClipNode::ClipNode(const ClipNode& cn, const CopyOp& copyop):
    Group(cn,copyop),
    _value(cn._value),
    _referenceFrame(cn._referenceFrame)
{
    setStateSet(new StateSet);
#if !(defined(OSG_GL_FIXED_FUNCTION_AVAILABLE) && !defined(OSG_GLES1_AVAILABLE))
    _setUniforms();
#endif
    for(ClipPlaneList::const_iterator itr=cn._planes.begin();
        itr!=cn._planes.end();
        ++itr)
    {
        ClipPlane* plane = dynamic_cast<ClipPlane*>(copyop(itr->get()));
        if (!plane)
            continue;
        _planes.push_back(plane);
        _stateset->setAssociatedModes(plane, _value);
    }
}

ClipNode::~ClipNode()
{
}

void ClipNode::setReferenceFrame(ReferenceFrame rf)
{
    _referenceFrame = rf;
}

// Create a 6 clip planes to create a clip box.
void ClipNode::createClipBox(const BoundingBox& bb,unsigned int clipPlaneNumberBase)
{
    _planes.clear();
    if (!_stateset.valid()) _stateset = new osg::StateSet;

    _planes.push_back(new ClipPlane(clipPlaneNumberBase  ,1.0,0.0,0.0,-bb.xMin()));
    _stateset->setAssociatedModes(_planes.back().get(), _value);
    _planes.push_back(new ClipPlane(clipPlaneNumberBase+1,-1.0,0.0,0.0,bb.xMax()));
    _stateset->setAssociatedModes(_planes.back().get(), _value);

    _planes.push_back(new ClipPlane(clipPlaneNumberBase+2,0.0,1.0,0.0,-bb.yMin()));
    _stateset->setAssociatedModes(_planes.back().get(), _value);
    _planes.push_back(new ClipPlane(clipPlaneNumberBase+3,0.0,-1.0,0.0,bb.yMax()));
    _stateset->setAssociatedModes(_planes.back().get(), _value);

    _planes.push_back(new ClipPlane(clipPlaneNumberBase+4,0.0,0.0,1.0,-bb.zMin()));
    _stateset->setAssociatedModes(_planes.back().get(), _value);
    _planes.push_back(new ClipPlane(clipPlaneNumberBase+5,0.0,0.0,-1.0,bb.zMax()));
    _stateset->setAssociatedModes(_planes.back().get(), _value);
}

// Add a ClipPlane to a ClipNode. Return true if plane is added,
// return false if plane already exists in ClipNode, or clipplane is false.
bool ClipNode::addClipPlane(ClipPlane* clipplane)
{
    if (!clipplane) return false;

    if (std::find(_planes.begin(),_planes.end(),clipplane)==_planes.end())
    {
        // cliplane doesn't exist in list so add it.
        _planes.push_back(clipplane);
        if (!_stateset.valid()) _stateset = new osg::StateSet;
        _stateset->setAssociatedModes(clipplane, _value);
        return true;
    }
    else
    {
        return false;
    }
}

// Remove ClipPlane from a ClipNode. Return true if plane is removed,
// return false if plane does not exists in ClipNode.
bool ClipNode::removeClipPlane(ClipPlane* clipplane)
{
    if (!clipplane) return false;

    ClipPlaneList::iterator itr = std::find(_planes.begin(),_planes.end(),clipplane);
    if (itr!=_planes.end())
    {
        // cliplane exist in list so erase it.
        _stateset->removeAssociatedModes(clipplane);
        _planes.erase(itr);
        return true;
    }
    else
    {
        return false;
    }
}

// Remove ClipPlane, at specified index, from a ClipNode. Return true if plane is removed,
// return false if plane does not exists in ClipNode.
bool ClipNode::removeClipPlane(unsigned int pos)
{
    if (pos<_planes.size())
    {
        ClipPlaneList::iterator itr = _planes.begin();
        std::advance(itr, pos);
        _stateset->removeAssociatedModes(itr->get());
        _planes.erase(itr);
        return true;
    }
    else
    {
        return false;
    }
}

// Set the GLModes on StateSet associated with the ClipPlanes.
void ClipNode::setStateSetModes(StateSet& stateset,StateAttribute::GLModeValue value) const
{
    for(ClipPlaneList::const_iterator itr=_planes.begin();
        itr!=_planes.end();
        ++itr)
    {
        stateset.setAssociatedModes(itr->get(),value);
    }
}

void ClipNode::setLocalStateSetModes(StateAttribute::GLModeValue value)
{
    _value = value;
    if (!_stateset) setStateSet(new StateSet);

    setStateSetModes(*_stateset,value);
}

BoundingSphere ClipNode::computeBound() const
{
    return Group::computeBound();
}

#if !(defined(OSG_GL_FIXED_FUNCTION_AVAILABLE) && !defined(OSG_GLES1_AVAILABLE))
#define MAX_CLIP_PLANES 8

namespace
{

class ClipPlanesUpdate : public osg::NodeCallback
{
    public:
        ClipPlanesUpdate(ClipNode* clipNode):
            _clipNode(clipNode),
            _numPlanes(new osg::Uniform(osg::Uniform::UNSIGNED_INT, "osg_NumClipPlanes")),
            _clipPlanes(new osg::Uniform(osg::Uniform::FLOAT_VEC4, "osg_ClipPlanes", MAX_CLIP_PLANES))
        {
            // setting the data variance the DYNAMIC makes it safe to change
            // the uniform values during the CULL traversal.
            _numPlanes->setDataVariance(osg::Object::DYNAMIC);
            _clipPlanes->setDataVariance(osg::Object::DYNAMIC);

            StateSet* stateSet = clipNode->getOrCreateStateSet();
            stateSet->addUniform(_numPlanes);
            stateSet->addUniform(_clipPlanes);
        }

    public:
        virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>(nv);

            const osg::Matrix modelViewTransInv = osg::Matrix::inverse(*cv->getModelViewMatrix());

            unsigned int numPlanes = 0;
            for (osg::ClipNode::ClipPlaneList::const_iterator itr = _clipNode->getClipPlaneList().begin();
                 itr != _clipNode->getClipPlaneList().end();
                ++itr)
            {
                const unsigned int planeNum = itr->get()->getClipPlaneNum();
                if (planeNum > MAX_CLIP_PLANES)
                {
                    OSG_WARN <<"Warning ClipNode: clip plane index greater than maximum " << MAX_CLIP_PLANES << std::endl;
                }
                else
                {
                    const Vec4d v = modelViewTransInv * itr->get()->getClipPlane();
                    _clipPlanes->setElement(planeNum, Vec4(v));
                    numPlanes = std::max(planeNum + 1, numPlanes);
                }
                // Unset planes are going to get undefined values, but it
                // doesn't matter. For these planes the shaders are going to
                // do useless work writing to the gl_ClipDistance array, but
                // these undefined writes won't affect the outcome because
                // the array positions are disabled (though GL_CLIP_DISTANCEX).
                // It's the user's reponsibility not to leave gaps in the
                // array when performance matters.
            }
            _numPlanes->set(numPlanes);

            traverse(node, nv);
        }

    protected:
        const ClipNode* _clipNode;
        ref_ptr<Uniform> _numPlanes;
        ref_ptr<Uniform> _clipPlanes;
};
}

void ClipNode::_setUniforms()
{
    setCullCallback(new ClipPlanesUpdate(this));
}
#endif
