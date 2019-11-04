/******************************************************************************
** Copyright (c) 2018 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file OcclusionQueryNode.cpp
//! \ingroup vrvOsg
//! \brief Contains makVrv::OcclusionQueryNode class.

#include <osg/OcclusionQueryNode>
//#include <vrvOsg/DtOsgRenderer.h>
//#include <vrvOsg/DtImguiDrawable.h>
//#include <vrvOsg/DtOsgLightManager.h>
//#include <osgEarth/VirtualProgram>
//#include <osgEarth/CullingUtils>

//#include <vrvImgui/imgui.h>

#include <iostream>
#include <sstream>
//#include <boost/functional/hash.hpp>

#include <OpenThreads/ScopedLock>
#include <osg/Timer>
#include <osg/Notify>
#include <osg/CopyOp>
#include <osg/Vec3>
#include <osg/MatrixTransform>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/BoundingBox>
#include <osg/BoundingSphere>
#include <osg/Referenced>
#include <osg/ComputeBoundsVisitor>
#include <osg/StateSet>
#include <osg/StateAttribute>
#include <osg/PolygonMode>
#include <osg/ColorMask>
#include <osg/PolygonOffset>
#include <osg/Depth>
#include <osg/RenderInfo>
#include <osg/LightSource>

#include <osg/ConcurrencyViewerMacros>
#include <osgSim/LightPointNode>
#include <osgUtil/RenderLeaf>
#include <osgUtil/CullVisitor>

#include <map>
#include <vector>
#include <osgUtil/ShaderGen>

#include <OpenThreads/Thread>

#include <osgDB/ObjectWrapper>
#include <osgDB/InputStream>
#include <osgDB/OutputStream>
#include <osgDB/Serializer>


namespace osg
{

   osg::ref_ptr<osg::StateSet>  OcclusionQueryNode::_gs_stateset;
   osg::ref_ptr<osg::StateSet>  OcclusionQueryNode::_gs_queryStateset;
   osg::ref_ptr<osg::StateSet>  OcclusionQueryNode::_gs_debugStateset;

   class OQNComputeBoundsVisitor : public osg::ComputeBoundsVisitor
   {
   public:
      virtual void apply(osg::LightSource & lightSource)
      {

         // This doesn't really work
         osg::BoundingSphere sphere = lightSource.computeBound();
         osg::Light * light = lightSource.getLight();
         // stolen from OsgLightManager::registerLight 

         if (light->getLinearAttenuation() > 1.0f && light->getQuadraticAttenuation() > 1.0f)
         {
            float fStartDistance = light->getLinearAttenuation();
            static_cast<void>(fStartDistance);
            float fEndDistance = light->getQuadraticAttenuation();

            sphere._radius = fEndDistance;
         }
         else
         {
            // Set it to a reasonable value
            sphere._radius = 20.0f;
         }

         if (_matrixStack.empty()) {
            _bb.expandBy(sphere);
         }
         else if (sphere.valid())
         {
            const osg::Matrix& matrix = _matrixStack.back();
            osg::Vec3 offset = sphere.center() * matrix;
            sphere._center = offset;
            _bb.expandBy(sphere);

         }

      }

      /* virtual void apply(osgSim::LightPointNode & lightSource) {
      osg::BoundingSphere sphere = lightSource.computeBound();

      if (_matrixStack.empty()) {
      _bb.expandBy(sphere);
      }
      else if (sphere.valid())
      {

      const osg::Matrix& matrix = _matrixStack.back();
      float rad = sphere.radius();
      _bb.expandBy(sphere.center() + osg::Vec3(rad, rad, rad) * matrix);
      _bb.expandBy(sphere.center() + osg::Vec3(rad, rad, -rad) * matrix);

      _bb.expandBy(sphere.center() + osg::Vec3(-rad, rad, rad) * matrix);
      _bb.expandBy(sphere.center() + osg::Vec3(-rad, rad, -rad) * matrix);

      _bb.expandBy(sphere.center() + osg::Vec3(rad, -rad, rad) * matrix);
      _bb.expandBy(sphere.center() + osg::Vec3(rad, -rad, -rad) * matrix);

      _bb.expandBy(sphere.center() + osg::Vec3(-rad, -rad, rad) * matrix);
      _bb.expandBy(sphere.center() + osg::Vec3(-rad, -rad, -rad) * matrix);
      }
      }*/
   };



   //
   // Copyright (C) 2007 Skew Matrix Software LLC (http://www.skew-matrix.com)
   //
   // This library is open source and may be redistributed and/or modified under
   // the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
   // (at your option) any later version.  The full license is in LICENSE file
   // included with this distribution, and on the openscenegraph.org website.
   //
   // This library is distributed in the hope that it will be useful,
   // but WITHOUT ANY WARRANTY; without even the implied warranty of
   // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   // OpenSceneGraph Public License for more details.
   //

   //
   // Support classes, used by (and private to) OcclusionQueryNode.
   //   (Note a lot of this is historical. OcclusionQueryNode formaerly
   //   existed as a NodeKit outside the core OSG distribution. Many
   //   of these classes existed in their own separate header and
   //   source files.)


   int OcclusionQueryNode::_gs_num_passed = 0;
   int OcclusionQueryNode::_gs_num_culled = 0;
   int OcclusionQueryNode::_gs_debugBBOverridge = -1;
   bool OcclusionQueryNode::_gs_useReverseZ = false;

   // Create and return a StateSet appropriate for performing an occlusion
   //   query test (disable lighting, texture mapping, etc). Probably some
   //   room for improvement here. Could disable shaders, for example.
   osg::StateSet* OcclusionQueryNode::initOQGeometryState(bool useReverseZ)
   {
      osg::StateSet* state = new osg::StateSet;
      state->setName("shared_occulsion_query_ss");

      // TBD Possible bug, need to allow user to set render bin number.
/*
      state->setRenderBinDetails(DtOsgRenderer::OCCLUSION_RENDERBIN, DtOsgRenderer::OcclusionRenderBin,
         osg::StateSet::OVERRIDE_PROTECTED_RENDERBIN_DETAILS);
*/
      state->setNestRenderBins(false);

      state->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
      state->setMode(GL_CULL_FACE, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

      osg::ColorMask* cm = new osg::ColorMask(false, false, false, false);
      state->setAttributeAndModes(cm, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

      if (useReverseZ) {
         osg::Depth* d = new osg::Depth(osg::Depth::GEQUAL, 0.f, 1.f, false);
         state->setAttributeAndModes(d, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
         osg::PolygonOffset* po = new osg::PolygonOffset(1., 1.);
         state->setAttributeAndModes(po, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
      }
      else {
         osg::Depth* d = new osg::Depth(osg::Depth::LEQUAL, 0.f, 1.f, false);
         state->setAttributeAndModes(d, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
         osg::PolygonOffset* po = new osg::PolygonOffset(-1., -1.);
         state->setAttributeAndModes(po, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
      }

      osg::PolygonMode* pm = new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL);
      state->setAttributeAndModes(pm, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

      return state;
   }

   // Create and return a StateSet for rendering a debug representation of query geometry.
   osg::StateSet* OcclusionQueryNode::initOQDebugState(bool reverse_z)
   {
      osg::StateSet* debugState = new osg::StateSet;
      debugState->setName("shared_occulsion_debug_ss");

//      debugState->setRenderBinDetails(DtOsgRenderer::OCCLUSION_RENDERBIN, DtOsgRenderer::OcclusionRenderBin,
//         osg::StateSet::OVERRIDE_PROTECTED_RENDERBIN_DETAILS);
      debugState->setNestRenderBins(false);

      debugState->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
      debugState->setMode(GL_CULL_FACE, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

      osg::PolygonMode* pm = new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE);
      //osg::PolygonMode* pm = new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL);
      debugState->setAttributeAndModes(pm, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

      if (reverse_z) {
          osg::PolygonOffset* po = new osg::PolygonOffset(1., 1.);
          debugState->setAttributeAndModes(po, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
      }
      else {
          osg::PolygonOffset* po = new osg::PolygonOffset(-1., -1.);
          debugState->setAttributeAndModes(po, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
      }

      return debugState;
   }

   osg::StateSet* OcclusionQueryNode::initOQState()
   {
      osg::StateSet* ss = new osg::StateSet();
      ss->setName("shared_occulsion_node_ss");
      return ss;
   }

   // VRV_PATCH added to be able to fix high precision rendering issues.
   class DebugGeometry : public osg::Geometry
   {
   public:
      DebugGeometry(OcclusionQueryNode & parent, const std::string& oqnName = std::string("")) :
         osg::Geometry(),
         _queryNode(parent),
         _oqnName(oqnName)
      {
         ;
      }

      virtual void drawImplementation(osg::RenderInfo& renderInfo) const;

   protected:

      // Needed for debug only
      std::string _oqnName;
      OcclusionQueryNode & _queryNode;
   };

   void DebugGeometry::drawImplementation(osg::RenderInfo& renderInfo) const
   {
      osg::State* renderInfoState = renderInfo.getState();

      osg::Matrix old = renderInfoState->getModelViewMatrix();
      osg::Matrix newMat = _queryNode.getBBoxTransform() * old;
      renderInfoState->applyModelViewMatrix(newMat);
      renderInfoState->applyModelViewAndProjectionUniformsIfRequired();

      Geometry::drawImplementation(renderInfo);

      renderInfoState->applyModelViewMatrix(old);
      renderInfoState->applyModelViewAndProjectionUniformsIfRequired();


   }

   void RetrieveQueriesCallback::retrieveQueries(osg::RenderInfo &renderInfo) const
   {

      if (_results.empty())
         return;

      osg::CVMarkerSeries series("Render Tasks");
      osg::CVSpan allocSpan(series, 4, "RetrieveQueries");
      series.write_alert("%i queries", _results.size());

      const osg::Camera& camera = *renderInfo.getCurrentCamera();

      const osg::Timer& timer = *osg::Timer::instance();
      osg::Timer_t start_tick = timer.tick();
      double elapsedTime(0.);
      int count(0);

      const osg::GLExtensions* ext = 0;
      if (camera.getGraphicsContext())
      {
         // The typical path, for osgViewer-based applications or any
         //   app that has set up a valid GraphicsCOntext for the Camera.
         ext = camera.getGraphicsContext()->getState()->get<osg::GLExtensions>();
      }
      else
      {
         // No valid GraphicsContext in the Camera. This might happen in
         //   SceneView-based apps. Rely on the creating code to have passed
         //   in a valid GLExtensions pointer, and hope it's valid for any
         //   context that might be current.
         OSG_DEBUG << "osgOQ: RQCB: Using fallback path to obtain GLExtensions pointer." << std::endl;
         ext = _extensionsFallback;
         if (!ext)
         {
            ext = renderInfo.getState()->get<osg::GLExtensions>();

            if (!ext) {
               OSG_FATAL << "osgOQ: RQCB: GLExtensions pointer fallback is NULL." << std::endl;
               return;
            }
         }
      }
      ext->glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, -1, "RetrieveQueriesCallback");

      ResultsVector::const_iterator it = _results.begin();
      while (it != _results.end())
      {
         osg::TestResult* tr = const_cast<osg::TestResult*>((*it).get());

         if (!tr->_active || !tr->_init)
         {
            // This test wasn't executed last frame. This is probably because
            //   a parent node failed the OQ test, this node is outside the
            //   view volume, or we didn't run the test because we had not
            //   exceeded visibleQueryFrameCount.
            // Do not obtain results from OpenGL.
            ++it;
            continue;
         }

         OSG_DEBUG <<
            "osgOQ: RQCB: Retrieving..." << std::endl;

         GLint ready = 0;
         ext->glGetQueryObjectiv(tr->_id, GL_QUERY_RESULT_AVAILABLE, &ready);
         if (ready)
         {
            //float last_anwser = tr->_numPixels;
            ext->glGetQueryObjectiv(tr->_id, GL_QUERY_RESULT, &(tr->_numPixels));

            if (tr->_numPixels < 0)
               OSG_WARN << "osgOQ: RQCB: " <<
               "glGetQueryObjectiv returned negative value (" << tr->_numPixels << ")." << std::endl;

            // Either retrieve last frame's results, or ignore it because the
            //   camera is inside the view. In either case, _active is now false.
            tr->_active = false;
            tr->_numQueriesRun++;
         }
         // else: query result not available yet, try again next frame
         ++it;
         count++;
      }

      ext->glPopDebugGroup();

      elapsedTime = timer.delta_s(start_tick, timer.tick());
//      VRV_PATCH too much spew
//      OSG_INFO << "osgOQ: RQCB: " << "Retrieved " << count <<
//         " queries in " << elapsedTime << " seconds." << std::endl;
   }

   void RetrieveQueriesCallback::operator () (osg::RenderInfo& renderInfo) const
   {
      retrieveQueries(renderInfo);

      if (myNextCallback.get()) {
         myNextCallback.get()->operator()(renderInfo);
      }
   }

   void ClearQueriesCallback::operator() (osg::RenderInfo& renderInfo) const
   {
      if (!_rqcb)
      {
         OSG_FATAL << "osgOQ: CQCB: Invalid RQCB." << std::endl;
         return;
      }

      {
         osg::CVMarkerSeries series("Render Tasks");
         osg::CVSpan allocSpan(series, 4, "ClearQueries");
         _rqcb->reset();
      }
      if (myNextCallback.get()) {
         myNextCallback.get()->operator()(renderInfo);
      }

   }


   // static cache of deleted query objects which can only
   // be completely deleted once the appropriate OpenGL context
   // is set.
   typedef std::list< GLuint > QueryObjectList;
   typedef osg::buffered_object< QueryObjectList > DeletedQueryObjectCache;

   static OpenThreads::Mutex s_mutex_deletedQueryObjectCache;
   static DeletedQueryObjectCache s_deletedQueryObjectCache;



   QueryGeometry::QueryGeometry(OcclusionQueryNode & parent, const std::string& oqnName)
      : _queryNode(parent), _oqnName(oqnName)
   {
      // TBD check to see if we can have this on.
      setUseDisplayList(false);
   }

   QueryGeometry::~QueryGeometry()
   {
      reset();
   }


   void
      QueryGeometry::reset()
   {
      OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mapMutex);

      ResultsMap::iterator it = _results.begin();
      while (it != _results.end())
      {
         osg::ref_ptr<osg::TestResult> tr = it->second;
         if (tr->_init) {
            QueryGeometry::deleteQueryObject(tr->_contextID, tr->_id);
            tr->_id = 0;
            tr->_init = false;
         }
         ++it;
      }
      _results.clear();
   }

   // After 1.2, param 1 changed from State to RenderInfo.
   // Warning: Version was still 1.2 on dev branch long after the 1.2 release,
   //   and finally got bumped to 1.9 in April 2007.
   void
      QueryGeometry::drawImplementation(osg::RenderInfo& renderInfo) const
   {
      osg::State* renderInfoState = renderInfo.getState();

      unsigned int contextID = renderInfoState->getContextID();
      osg::GLExtensions* ext = renderInfoState->get<osg::GLExtensions>();

      if (!ext->isARBOcclusionQuerySupported && !ext->isOcclusionQuerySupported)
         return;

      osg::Camera* cam = renderInfo.getCurrentCamera();

      RetrieveQueriesCallback* rqcb = dynamic_cast<
         RetrieveQueriesCallback*>(cam->getPostDrawCallback());
      if (!rqcb)
      {
         //         OSG_FATAL << "OcclusionQueryNode: Invalid RetrieveQueriesCallback." << std::endl;
         return;
      }

      // Add callbacks if necessary.
      // isn't done automatically now
      /*if (!cam->getPostDrawCallback())
      {
      RetrieveQueriesCallback* rqcb = new RetrieveQueriesCallback(ext);
      cam->setPostDrawCallback(rqcb);

      ClearQueriesCallback* cqcb = new ClearQueriesCallback;
      cqcb->_rqcb = rqcb;
      cam->setPreDrawCallback(cqcb);
      }*/
      osgUtil::RenderLeaf * leaf = renderInfo.getState()->getCurrentRenderLeaf();

      // Get TestResult from Camera map
      osg::ref_ptr<osg::TestResult> tr;
      {
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mapMutex);
         std::map< int, long long int>::iterator iter = _traversalToNodePathMap.find(leaf->_traversalOrderNumber);
         long long int nodepathhash = 0;
         if (iter != _traversalToNodePathMap.end())
         {
            nodepathhash = iter->second;
            _traversalToNodePathMap.erase(iter);
            tr = (_results[NodePathAndCamera(nodepathhash, cam)]);
         }

         if (!tr.valid())
         {
            tr = new osg::TestResult;
            _results[NodePathAndCamera(nodepathhash, cam)] = tr;
         }
      }


      // Issue query
      if (!tr->_init)
      {
         ext->glGenQueries(1, &(tr->_id));
         tr->_contextID = contextID;
         tr->_init = true;
      }

      if (tr->_active)
      {
         // last query hasn't been retrieved yet
         return;
      }
      ext->glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, -1, "DrawQuery");

      // Add TestResult to RQCB.
      rqcb->add(tr.get());


      OSG_DEBUG <<
         "OcclusionQueryNode: QG: Querying for: " << _oqnName << std::endl;

      osg::Matrix old = renderInfoState->getModelViewMatrix();
      osg::Matrix local_offset = _queryNode.getBBoxTransform() * old;
      renderInfoState->applyModelViewMatrix(local_offset);
      renderInfoState->applyModelViewAndProjectionUniformsIfRequired();

      ext->glBeginQuery(GL_SAMPLES_PASSED_ARB, tr->_id);

      osg::Geometry::drawImplementation(renderInfo);
      ext->glEndQuery(GL_SAMPLES_PASSED_ARB);

      renderInfoState->applyModelViewMatrix(old);
      renderInfoState->applyModelViewAndProjectionUniformsIfRequired();

      osg::FrameStamp * fs = renderInfo.getView()->getFrameStamp();
      tr->_lastFinishedQueryFrame = tr->_queryFrame;
      tr->_queryFrame = fs->getFrameNumber();
      tr->_active = true;


      OSG_DEBUG <<
         "OcclusionQueryNode: QG. OQNName: " << _oqnName <<
         ", Ctx: " << contextID <<
         ", ID: " << tr->_id << std::endl;
#ifdef _DEBUG
      {
         GLenum err;
         if ((err = glGetError()) != GL_NO_ERROR)
         {
            OSG_FATAL << "OcclusionQueryNode: QG: OpenGL error: " << err << "." << std::endl;
         }
      }
#endif
      ext->glPopDebugGroup();


   }


   unsigned int
      QueryGeometry::getNumPixels(NodePathAndCamera & npc, const int currentTraversalFrame, int & query_frame, bool & active)
   {

      osg::ref_ptr<osg::TestResult> tr;
      {
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mapMutex);
         tr = _results[npc];
         if (!tr.valid())
         {
            tr = new osg::TestResult;
            _results[npc] = tr;
         }
      }

      active = tr->_active;
      if (active)
      {
         query_frame = tr->_lastFinishedQueryFrame;
         if ((currentTraversalFrame - tr->_lastFinishedQueryFrame) > 60) {
            tr->_numQueriesRun = 0;
         }
         if (tr->_numQueriesRun < 5) {
            return 10000;
         }
         return tr->_numPixels;
      }
      else {
         query_frame = tr->_queryFrame;
         if (tr->_numQueriesRun  < 5) {
            return 10000;
         }
      }
      return tr->_numPixels;
   }


   void
      QueryGeometry::releaseGLObjects(osg::State* state) const
   {
      if (!state)
      {
         // delete all query IDs for all contexts.
         const_cast<QueryGeometry*>(this)->reset();
      }
      else
      {
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mapMutex);

         // Delete all query IDs for the specified context.
         unsigned int contextID = state->getContextID();
         ResultsMap::iterator it = _results.begin();
         while (it != _results.end())
         {
            osg::ref_ptr<osg::TestResult> tr = it->second;
            if (tr->_contextID == contextID)
            {
               QueryGeometry::deleteQueryObject(contextID, tr->_id);
               tr->_id = 0;
               tr->_init = false;
            }
            ++it;
         }
      }
   }

   void
      QueryGeometry::deleteQueryObject(unsigned int contextID, GLuint handle)
   {
      if (handle != 0)
      {
         OpenThreads::ScopedLock< OpenThreads::Mutex > lock(s_mutex_deletedQueryObjectCache);

         // insert the handle into the cache for the appropriate context.
         s_deletedQueryObjectCache[contextID].push_back(handle);
      }
   }


   void
      QueryGeometry::flushDeletedQueryObjects(unsigned int contextID, double /*currentTime*/, double& availableTime)
   {
      // if no time available don't try to flush objects.
      if (availableTime <= 0.0) return;

      const osg::Timer& timer = *osg::Timer::instance();
      osg::Timer_t start_tick = timer.tick();
      double elapsedTime = 0.0;

      {
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_mutex_deletedQueryObjectCache);

         const osg::GLExtensions* extensions = osg::GLExtensions::Get(contextID, false);

         QueryObjectList& qol = s_deletedQueryObjectCache[contextID];

         for (QueryObjectList::iterator titr = qol.begin();
            titr != qol.end() && elapsedTime < availableTime;
            )
         {
            if (extensions) {
               extensions->glDeleteQueries(1L, &(*titr));
            }
            titr = qol.erase(titr);
            elapsedTime = timer.delta_s(start_tick, timer.tick());
         }
      }

      availableTime -= elapsedTime;
   }

   void
      QueryGeometry::discardDeletedQueryObjects(unsigned int contextID)
   {
      OpenThreads::ScopedLock< OpenThreads::Mutex > lock(s_mutex_deletedQueryObjectCache);
      QueryObjectList& qol = s_deletedQueryObjectCache[contextID];
      qol.clear();
   }

   // End support classes
   //


   bool OcclusionQueryNode::theUseOcclusionNodes = false;

   OpenThreads::Mutex  OcclusionQueryNode::_initializationLock;

   OpenThreads::Mutex& OcclusionQueryNode::initializationLock()
   {
       return _initializationLock;
   }

   OcclusionQueryNode::OcclusionQueryNode()
      : _enabled(true),
      _passed(false),
      _visThreshold(500),
      _queryFrameCount(5),
      _debugBB(false),
      _bboxTransform(osg::Matrix::identity()),
      _padding(osg::Vec3(1, 1, 1))
   {
      OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_initializationLock);
      if (!_gs_debugStateset.get()) {
         _gs_debugStateset = initOQDebugState(_gs_useReverseZ);
      }

      if (!_gs_queryStateset.get()) {
         _gs_queryStateset = initOQGeometryState(_gs_useReverseZ);
      }

      // create deg shader so that it works with viewport multicast shadows
      if (!_gs_stateset.get()) {
         _gs_stateset = initOQState();
      }
       
       // OQN has two Geode member variables, one for doing the
      //   query and one for rendering the debug geometry.
      //   Create and initialize them.
      createSupportNodes();
   }

   OcclusionQueryNode::~OcclusionQueryNode()
   {
   }

   OcclusionQueryNode::OcclusionQueryNode(const OcclusionQueryNode& oqn, const osg::CopyOp& copyop)
      : osg::Group(oqn, copyop),
      _passed(false)
   {
      OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_initializationLock);

      _enabled = oqn._enabled;
      _visThreshold = oqn._visThreshold;
      _queryFrameCount = oqn._queryFrameCount;
      _debugBB = oqn._debugBB;
      _bboxTransform = osg::Matrix::identity();
      _padding = oqn._padding;

      // Regardless of shallow or deep, create unique support nodes.
      createSupportNodes();
   }


   bool OcclusionQueryNode::getPassed(const osg::Camera* camera, osg::NodeVisitor& nv)
   {
      if (!_enabled || !getUseOcclusionNodes()) 
      {
         // Queries are not enabled. The caller should be osgUtil::CullVisitor,
         //   return true to traverse the subgraphs.
         _gs_num_passed++;
         return true;
      }

      size_t ptr_val = calcPathHash(nv);
      

      NodePathAndCamera nodepathhash(ptr_val, camera);
      unsigned int currentTraversalFrame = nv.getTraversalNumber();
      {
         // Two situations where we want to simply do a regular traversal:
         //  1) it's the first frame for this camera
         //  2) we haven't rendered for an abnormally long time (probably because we're an out-of-range LOD child)
         // In these cases, assume we're visible to avoid blinking.
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_frameCountMutex);
         const unsigned int& lastQueryFrame(_frameCountMap[nodepathhash]);
         if ((lastQueryFrame == 0) ||
            ((currentTraversalFrame - lastQueryFrame) > (_queryFrameCount + 1)))
         {
            _gs_num_passed++;
            return true;
         }
      }

      if (_queryGeode->getDrawable(0) == NULL)
      {
         OSG_FATAL << "OcclusionQueryNode: OcclusionQueryNode: No QueryGeometry." << std::endl;
         // Something's broke. Return true so we at least render correctly.
         return true;
      }
      QueryGeometry* qg = static_cast<QueryGeometry*>(_queryGeode->getDrawable(0));

      // Get the near plane for the upcoming distance calculation.
      osg::Matrix::value_type nearPlane;
      const osg::Matrix& proj(camera->getProjectionMatrix());
      if ((proj(3, 3) != 1.) || (proj(2, 3) != 0.) || (proj(1, 3) != 0.) || (proj(0, 3) != 0.))
         nearPlane = proj(3, 2) / (proj(2, 2) - 1.);  // frustum / perspective
      else
         nearPlane = (proj(3, 2) + 1.) / proj(2, 2);  // ortho

                                                      // If the distance from the near plane to the bounding sphere shell is positive, retrieve
                                                      //   the results. Otherwise (near plane inside the BS shell) we are considered
                                                      //   to have passed and don't need to retrieve the query.
      const osg::BoundingSphere& bs = getBound();
      osg::Matrix::value_type distanceToEyePoint = nv.getDistanceToEyePoint(bs._center, false);

      osg::Matrix::value_type distance = distanceToEyePoint - nearPlane - bs._radius;
      // VRV_PATCH
      _passed = (distance <= 5.0);
      if (_passed)
      {
         // inside volume
         _gs_num_passed++;
         return _passed;
      }

      int queryFrame = 0;
      bool active = false;
      int result = qg->getNumPixels(nodepathhash, currentTraversalFrame, queryFrame, active);

      _passed = ((unsigned int)(result) > _visThreshold);

      if (_passed) {
         _gs_num_passed++;
      }
      else {
         _gs_num_culled++;
      }

      /*static const  osg::Camera* init_camera = camera;
      if (init_camera == camera) {
      if (result < 100) {
      setDebugBoxColor(osg::Vec4(1, 0, 0, 1));
      }
      }*/

      return _passed;

   }

   void OcclusionQueryNode::traverseQuery(const osg::Camera* camera, osgUtil::CullVisitor& cv)
   {
/*
      if (!_enabled) {
         return;
      }

      size_t ptr_val = calcPathHash(cv);

      NodePathAndCamera nodepathhash(ptr_val, camera);
      bool issueQuery = true;;
      {
         const int curFrame = cv.getTraversalNumber();

         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_frameCountMutex);
         unsigned int& lastQueryFrame = _frameCountMap[nodepathhash];
         issueQuery = (curFrame - lastQueryFrame >= _queryFrameCount);
         // VRV_PATCH GNP
         if (issueQuery) {
            _frameCountMap[nodepathhash] = curFrame;
            if (lastQueryFrame == 0) {
               lastQueryFrame = curFrame;
            }
         }
      }

      if (issueQuery)
      {

         QueryGeometry * geom = dynamic_cast<QueryGeometry *>(_queryGeode->getDrawable(0));
         geom->addTraversalToNodepath(cv.getTraversalOrderNumber(), ptr_val);
         _queryGeode->accept(cv);

      }
      */
   }

   void OcclusionQueryNode::traverseDebug(osgUtil::CullVisitor& cv)
   {
      if (_debugBB || _gs_debugBBOverridge == 1)
      {
         // If requested, display the debug geometry
         _debugGeode->accept(cv);

      }

   }

   osg::BoundingSphere OcclusionQueryNode::computeBound() const
   {

      // This is the logical place to put this code, but the method is const. Cast
      //   away constness to compute the bounding box and modify the query geometry.
      OcclusionQueryNode* nonConstThis = const_cast<OcclusionQueryNode*>(this);

      {
         // Need to make this routine thread-safe. Typically called by the update
         //   Visitor, or just after the update traversal, but could be called by
         //   an application thread or by a non-osgViewer application.
         OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_computeBoundMutex);

         OQNComputeBoundsVisitor cbv;
         nonConstThis->accept(cbv);
         osg::BoundingBox bb = cbv.getBoundingBox();

         //vrv_patch, use what's computed for tighter cull bounds to avoid false negatives that then 
         // leak into the occlusion test.
         nonConstThis->_worldSpaceBBox = bb;

         if (bb.valid())
         {
            osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array;
            v->resize(8);
            (*v)[0] = osg::Vec3(bb._min.x() - _padding.x(), bb._min.y() - _padding.y(), bb._min.z() - _padding.z());
            (*v)[1] = osg::Vec3(bb._max.x() + _padding.x(), bb._min.y() - _padding.y(), bb._min.z() - _padding.z());
            (*v)[2] = osg::Vec3(bb._max.x() + _padding.x(), bb._min.y() - _padding.y(), bb._max.z() + _padding.z());
            (*v)[3] = osg::Vec3(bb._min.x() - _padding.x(), bb._min.y() - _padding.y(), bb._max.z() + _padding.z());
            (*v)[4] = osg::Vec3(bb._max.x() + _padding.x(), bb._max.y() + _padding.y(), bb._min.z() - _padding.z());
            (*v)[5] = osg::Vec3(bb._min.x() - _padding.x(), bb._max.y() + _padding.y(), bb._min.z() - _padding.z());
            (*v)[6] = osg::Vec3(bb._min.x() - _padding.x(), bb._max.y() + _padding.y(), bb._max.z() + _padding.z());
            (*v)[7] = osg::Vec3(bb._max.x() + _padding.x(), bb._max.y() + _padding.y(), bb._max.z() + _padding.z());
            osg::Vec3 base = (*v)[0];//Vec3(0, 0, 0)
            for (int i = 0; i < 8; i++) {
               (*v)[i] -= base;
            }

            osg::Matrix matrix;
            matrix.makeTranslate(base);
            nonConstThis->setBBoxTransform(matrix);

            osg::Geometry* query_geom = static_cast<osg::Geometry*>(nonConstThis->_queryGeode->getDrawable(0));
            query_geom->setCullingActive(false);
            query_geom->setVertexArray(v.get());

            osg::Geometry* debug_geom = static_cast<osg::Geometry*>(nonConstThis->_debugGeode->getDrawable(0));
            debug_geom->setCullingActive(false);
            debug_geom->setVertexArray(v.get());

            osg::BoundingSphere bs;
            bs.set(cbv.getBoundingBox().center(), cbv.getBoundingBox().radius());
            return bs;
         }
      }
      nonConstThis->_enabled = false;
      nonConstThis->_worldSpaceBBox = osg::BoundingBox();
      return Group::computeBound();
   }


   // Should only be called outside of cull/draw. No thread issues.
   void OcclusionQueryNode::setQueriesEnabled(bool enable)
   {
      _enabled = enable;
   }

   // Should only be called outside of cull/draw. No thread issues.
   void OcclusionQueryNode::setDebugDisplay(bool debug)
   {
      _debugBB = debug;
   }

   bool OcclusionQueryNode::getDebugDisplay() const
   {
      return _debugBB;
   }

void OcclusionQueryNode::setQueryStateSet( StateSet* ss )
{
    if (!_queryGeode)
    {
        OSG_WARN << "osgOQ: OcclusionQueryNode:: Invalid query support node." << std::endl;
        return;
    }

    _queryGeode->setStateSet( ss );
}

StateSet* OcclusionQueryNode::getQueryStateSet()
{
    if (!_queryGeode)
    {
        OSG_WARN << "osgOQ: OcclusionQueryNode:: Invalid query support node." << std::endl;
        return NULL;
    }
    return _queryGeode->getStateSet();
}

const StateSet* OcclusionQueryNode::getQueryStateSet() const
{
    if (!_queryGeode)
    {
        OSG_WARN << "osgOQ: OcclusionQueryNode:: Invalid query support node." << std::endl;
        return NULL;
    }
    return _queryGeode->getStateSet();
}

void OcclusionQueryNode::setDebugStateSet( StateSet* ss )
{
    if (!_debugGeode)
    {
        OSG_WARN << "osgOQ: OcclusionQueryNode:: Invalid debug support node." << std::endl;
        return;
    }
    _debugGeode->setStateSet( ss );
}

StateSet* OcclusionQueryNode::getDebugStateSet()
{
    if (!_debugGeode.valid())
    {
        OSG_WARN << "osgOQ: OcclusionQueryNode:: Invalid debug support node." << std::endl;
        return NULL;
    }
    return _debugGeode->getStateSet();
}
const StateSet* OcclusionQueryNode::getDebugStateSet() const
{
    if (!_debugGeode.valid())
    {
        OSG_WARN << "osgOQ: OcclusionQueryNode:: Invalid debug support node." << std::endl;
        return NULL;
    }
    return _debugGeode->getStateSet();
}

   bool OcclusionQueryNode::getPassed() const
   {
      return _passed;
   }


   void OcclusionQueryNode::createSupportNodes()
   {
      GLushort indices[] = { 0, 1, 2, 3,  4, 5, 6, 7,
         0, 3, 6, 5,  2, 1, 4, 7,
         5, 4, 1, 0,  2, 7, 6, 3 };

      {
         // Add the test geometry Geode
         _queryGeode = new osg::Geode;
         _queryGeode->setName("OQTest");
         _queryGeode->setDataVariance(Object::DYNAMIC);

         osg::ref_ptr< QueryGeometry > geom = new QueryGeometry(*this, getName());
         geom->setDataVariance(Object::DYNAMIC);
         geom->addPrimitiveSet(new osg::DrawElementsUShort(osg::PrimitiveSet::QUADS, 24, indices));
         geom->setUseDisplayList(false);
         geom->setUseVertexBufferObjects(true);
         //geom->getOrCreateStateSet()->setAttribute(_sharedShader, osg::StateAttribute::ON);

         _queryGeode->addDrawable(geom.get());

         // create deg shader so that it works with veiwport multicast shadows
         _queryGeode->setStateSet(_gs_queryStateset);
      }

      {
         // Add a Geode that is a visual representation of the
         //   test geometry for debugging purposes

         _debugGeode = new osg::Geode;

         _debugGeode->setDataVariance(Object::DYNAMIC);

         osg::ref_ptr<osg::Geometry> geom = new DebugGeometry(*this, "debug");
         geom->setDataVariance(Object::DYNAMIC);
         geom->setUseDisplayList(false);

         osg::ref_ptr<osg::Vec4Array> ca = new osg::Vec4Array;
         ca->push_back(osg::Vec4(1.f, 1.f, 1.f, 1.0f));
         ca->setName(getName());
         geom->setColorArray(ca.get(), osg::Array::BIND_OVERALL);
         // rand()/(float)RAND_MAX * .5f + .5f, rand() / (float)RAND_MAX * .5f + .5f, rand() / (float)RAND_MAX * .5f + .5f, 1.f ) );

         geom->addPrimitiveSet(new osg::DrawElementsUShort(osg::PrimitiveSet::QUADS, 24, indices));

         _debugGeode->addDrawable(geom.get());
         _debugGeode->setStateSet(_gs_debugStateset);

      }

      setStateSet(_gs_stateset);
   }


   void OcclusionQueryNode::releaseGLObjects(osg::State* state) const
   {
      _queryGeode->releaseGLObjects(state);
      _debugGeode->releaseGLObjects(state);
      osg::Group::releaseGLObjects(state);
   }

   void OcclusionQueryNode::flushDeletedQueryObjects(unsigned int contextID, double currentTime, double& availableTime)
   {
      // Query object discard and deletion is handled by QueryGeometry support class.
      QueryGeometry::flushDeletedQueryObjects(contextID, currentTime, availableTime);
   }

   void OcclusionQueryNode::discardDeletedQueryObjects(unsigned int contextID)
   {
      // Query object discard and deletion is handled by QueryGeometry support class.
      QueryGeometry::discardDeletedQueryObjects(contextID);
   }

   QueryGeometry* OcclusionQueryNode::getQueryGeometry()
   {
      if (_queryGeode && _queryGeode->getDrawable(0))
      {
         QueryGeometry* qg = static_cast<QueryGeometry*>(_queryGeode->getDrawable(0));
         return qg;
      }
      return 0;
   }

   const QueryGeometry* OcclusionQueryNode::getQueryGeometry() const
   {
      if (_queryGeode && _queryGeode->getDrawable(0))
      {
         QueryGeometry* qg = static_cast<QueryGeometry*>(_queryGeode->getDrawable(0));
         return qg;
      }
      return 0;
   }

   void OcclusionQueryNode::setDebugBoxColor(const osg::Vec4 & color)
   {
      QueryGeometry* qg = static_cast<QueryGeometry*>(_queryGeode->getDrawable(0));
      osg::Geometry* geom = static_cast<osg::Geometry*>(_debugGeode->getDrawable(0));
      osg::ref_ptr<osg::Vec4Array> ca = new osg::Vec4Array;
      if (qg->getResults().size())
      {
         const osg::ref_ptr<osg::TestResult> tr = qg->getResults().begin()->second;
         if (tr) {
            osg::Vec4Array* array = dynamic_cast<osg::Vec4Array*>(geom->getColorArray());
            array->at(0) = color;
            geom->dirtyBound();
         }
      }
   }

   void OcclusionQueryNode::resetQueryCount(long long int nodepath, osg::Camera * camera)
   {
      QueryGeometry* qg = static_cast<QueryGeometry*>(_queryGeode->getDrawable(0));
      QueryGeometry::ResultsMap & results = qg->getResults();
      QueryGeometry::ResultsMap::iterator iter = results.find(NodePathAndCamera(nodepath, camera));
      if (iter != results.end())
      {
         const osg::ref_ptr<osg::TestResult>  & result = iter->second;
         if (result.valid() && result.get()->_numQueriesRun != 0) {
            result.get()->_numQueriesRun = 0;

         }
      }
   }

/*

   void EnableQueryVisitor::apply(osg::Group& oqn)
   {
      OcclusionQueryNode * qn = dynamic_cast<OcclusionQueryNode *>(&oqn);
      if (qn) {
         if (_enabled) {
            osg::Vec4 greenish(.7f, 1.0f, .7f, 1.0f);
            qn->setDebugBoxColor(greenish);
         }
         else {
            osg::Vec4 redish(1.0f, .7f, .7f, 1.0f);
            qn->setDebugBoxColor(redish);
         }
         qn->setQueriesEnabled(_enabled);
      }
      traverse(oqn);
   }
*/

/*

   REGISTER_OBJECT_WRAPPER(OcclusionQueryNode,
      new OcclusionQueryNode,
      OcclusionQueryNode,
      "osg::Object osg::Node osg::Group osg::OcclusionQueryNode")
   {
      ADD_BOOL_SERIALIZER(QueriesEnabled, true);  // _enabled
      ADD_UINT_SERIALIZER(VisibilityThreshold, 0);  // _visThreshold
      ADD_UINT_SERIALIZER(QueryFrameCount, 0);  // _queryFrameCount
      ADD_BOOL_SERIALIZER(DebugDisplay, false);  // _debugBB
      ADD_VEC3D_SERIALIZER(BoundsPadding, osg::Vec3(1.0, 1.0, 1.0));  // _Padding
   }*/


// End NodeVisitors

   
}
