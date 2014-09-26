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

#include <algorithm>

using namespace osg;

ClipNode::ClipNode():
    _value(StateAttribute::ON),
    _referenceFrame(RELATIVE_RF)
{
    setStateSet(new StateSet);
#if !(defined(OSG_GL_FIXED_FUNCTION_AVAILABLE) && !defined(OSG_GLES1_AVAILABLE))
    _setUniform();
#endif
}

ClipNode::ClipNode(const ClipNode& cn, const CopyOp& copyop):
    Group(cn,copyop),
    _value(cn._value),
    _referenceFrame(cn._referenceFrame)
{
    setStateSet(new StateSet);
#if !(defined(OSG_GL_FIXED_FUNCTION_AVAILABLE) && !defined(OSG_GLES1_AVAILABLE))
    _setUniform();
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
    class updateClipPlanes: public osg::Uniform::Callback
    {
    private:
    ClipNode * _clipNode;
    public:
    updateClipPlanes( ClipNode * clipNode) : _clipNode( clipNode ) {}

    virtual void operator()
        ( osg::Uniform* uniform, osg::NodeVisitor* nv )
        {
            OSG_NOTICE<<"UPDATING UNIFORM"<<std::endl;
            if(_clipNode->getNumClipPlanes() > MAX_CLIP_PLANES)
                OSG_WARN<<"Warning ClipNode: number clip planes greater than maximum "<<MAX_CLIP_PLANES<<std::endl;

            for(osg::ClipNode::ClipPlaneList::const_iterator itr=_clipNode->getClipPlaneList().begin();
                itr!=_clipNode->getClipPlaneList().end();
                ++itr)
            {
                if(itr->get()->getClipPlaneNum() > MAX_CLIP_PLANES)
                {
                    OSG_WARN<<"Warning ClipNode: index clip plane greater than maximum "<<MAX_CLIP_PLANES<<std::endl;
                }
                else
                {
                    uniform->setElement(itr->get()->getClipPlaneNum(), Vec4(itr->get()->getClipPlane()));
                    OSG_NOTICE<<itr->get()->getClipPlaneNum()<< " ";
                    OSG_NOTICE<<itr->get()->getClipPlane()[0]<< " ";
                    OSG_NOTICE<<itr->get()->getClipPlane()[1]<< " ";
                    OSG_NOTICE<<itr->get()->getClipPlane()[2]<< " ";
                    OSG_NOTICE<<itr->get()->getClipPlane()[3]<<std::endl;
                }
            }
        }
    };
}

void ClipNode::_setUniform()
{
    if(_clipPlanesUniform) return;

    _clipPlanesUniform = new osg::Uniform(osg::Uniform::FLOAT_VEC4, "osg_ClipPlanes", MAX_CLIP_PLANES);
    _maxClipPlanesUniform = new osg::Uniform(osg::Uniform::INT, "osg_MaxClipPlanes");
    _maxClipPlanesUniform->set( MAX_CLIP_PLANES );
    _clipPlanesUniform->setUpdateCallback( new updateClipPlanes( this ) );
    _stateset->addUniform( _clipPlanesUniform );
    _stateset->addUniform( _maxClipPlanesUniform );
    OSG_NOTICE<<"======================="<<_clipPlanesUniform->getNumElements()<<std::endl;
}
#endif
