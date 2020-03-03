#include <sstream>
#include <memory>
#ifndef WIN32
#include <strings.h>//for strncasecmp
#endif

//#include <vrvOsg/DtOsgPrintNodeHierarchyVisitor.h> // TO DEBUG IN VANTAGE
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/PositionAttitudeTransform>
#include <osg/Texture2D>
#include <osg/Version>
#include <osgDB/ConvertUTF>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include "fbxUtil.h"
#include "fbxMatrixTransformVisitor.h"

#if defined(_MSC_VER)
    #pragma warning( disable : 4505 )
    #pragma warning( default : 4996 )
#endif
#include <fbxsdk.h>

#include "ReaderWriterFBX.h"
#include "fbxReader.h"
#include "WriterNodeVisitor.h"



class CleanUpFbx
{
    FbxManager* m_pSdkManager;
public:
    explicit CleanUpFbx(FbxManager* pSdkManager) : m_pSdkManager(pSdkManager)
    {}

    ~CleanUpFbx()
    {
        m_pSdkManager->Destroy();
    }
};

void printSceneUnitInfo(FbxScene* pScene)
{
   if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::mm)
      OSG_INFO << "FBX in millimeters will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::dm)
      OSG_INFO << "FBX in decimeters will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::cm)
      OSG_INFO << "FBX in centimeters will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::km)
      OSG_INFO << "FBX in kilometers will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::Inch)
      OSG_INFO << "FBX in inches will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::Foot)
      OSG_INFO << "FBX in feet will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::Mile)
      OSG_INFO << "FBX in miles will be converted to meters" << std::endl;
   else if (pScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::Yard)
      OSG_INFO << "FBX in yards will be converted to meters" << std::endl;
   else
      OSG_INFO << "FBX unknown units" << std::endl;
}

void printAxisInfo(const FbxAxisSystem& fbxAxis)
{
   if (fbxAxis == FbxAxisSystem::MayaZUp)
      OSG_INFO << "FbxAxisSystem::MayaZUp" << std::endl;
   else if (fbxAxis == FbxAxisSystem::MayaYUp)
      OSG_INFO << "FbxAxisSystem::MayaYUp" << std::endl;
   else if (fbxAxis == FbxAxisSystem::Max)
      OSG_INFO << "FbxAxisSystem::Max" << std::endl;
   else if (fbxAxis == FbxAxisSystem::Motionbuilder)
      OSG_INFO << "FbxAxisSystem::Motionbuilder" << std::endl;
   else if (fbxAxis == FbxAxisSystem::OpenGL)
      OSG_INFO << "FbxAxisSystem::OpenGL" << std::endl;
   else if (fbxAxis == FbxAxisSystem::DirectX)
      OSG_INFO << "FbxAxisSystem::DirectX" << std::endl;
   else if (fbxAxis == FbxAxisSystem::Lightwave)
      OSG_INFO << "FbxAxisSystem::Lightwave" << std::endl;
   else
      OSG_INFO << "FbxAxisSystem:: ??? " << std::endl;
}

osgDB::ReaderWriter::ReadResult
ReaderWriterFBX::readNode(const std::string& filenameInit,
                          const osgDB::Options* options) const
{
    //making fbx loader single threaded. We have seen a crash when two FBX models
    // are loaded at the same time. 
    // This may not be needed when we move up to the latest version of the FBXSDK
    OpenThreads::ScopedLock<OpenThreads::ReentrantMutex> lock(fbxMutex);
    try
    {
        std::string ext(osgDB::getLowerCaseFileExtension(filenameInit));
        if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

        std::string filename(osgDB::findDataFile(filenameInit, options));
        if (filename.empty()) return ReadResult::FILE_NOT_FOUND;

        // keep the current path for possible xref files
        ReaderWriterFBX* thisReader = const_cast<ReaderWriterFBX*>(this);
        thisReader->currentFilePath = osgDB::getFilePath(filenameInit);

        FbxManager* pSdkManager = FbxManager::Create();

        if (!pSdkManager)
        {
            return ReadResult::ERROR_IN_READING_FILE;
        }

        CleanUpFbx cleanUpFbx(pSdkManager);

        pSdkManager->SetIOSettings(FbxIOSettings::Create(pSdkManager, IOSROOT));

        FbxScene* pScene = FbxScene::Create(pSdkManager, "");

        // The FBX SDK interprets the filename as UTF-8
#ifdef OSG_USE_UTF8_FILENAME
        const std::string& utf8filename(filename);
#else
        std::string utf8filename(osgDB::convertStringFromCurrentCodePageToUTF8(filename));
#endif

        FbxImporter* lImporter = FbxImporter::Create(pSdkManager, "");

        if (!lImporter->Initialize(utf8filename.c_str(), -1, pSdkManager->GetIOSettings()))
        {
#if FBXSDK_VERSION_MAJOR < 2014
            return std::string(lImporter->GetLastErrorString());
#else
            return std::string(lImporter->GetStatus().GetErrorString());
#endif
        }

        if (!lImporter->IsFBX())
        {
            return ReadResult::ERROR_IN_READING_FILE;
        }

        for (int i = 0; FbxTakeInfo* lTakeInfo = lImporter->GetTakeInfo(i); i++)
        {
            lTakeInfo->mSelect = true;
        }
        
        if (!lImporter->Import(pScene))
        {
#if FBXSDK_VERSION_MAJOR < 2014
            return std::string(lImporter->GetLastErrorString());
#else 
            return std::string(lImporter->GetStatus().GetErrorString());
#endif
        }
        
        // Convert to Max 3D coordinate system
        // We should only need this call to orient the scene
        // but it's not working correctly 
        //FbxAxisSystem::Max.ConvertScene(pScene);
        //FbxAxisSystem::MayaYUp.ConvertScene(pScene);
        
        // Converting to meters
        printSceneUnitInfo(pScene);
        FbxSystemUnit::m.ConvertScene(pScene);
        

        if (FbxNode* pNode = pScene->GetRootNode())
        {
            bool useFbxRoot = false;
            bool lightmapTextures = false;
            bool tessellatePolygons = false;
            bool zUp = true;
            if (options)
            {
                std::istringstream iss(options->getOptionString());
                std::string opt;
                while (iss >> opt)
                {
                    if (opt == "UseFbxRoot")
                    {
                        useFbxRoot = true;
                    }
                    if (opt == "LightmapTextures")
                    {
                        lightmapTextures = true;
                    }
                    if (opt == "TessellatePolygons")
                    {
                        tessellatePolygons = true;
                    }
                    if (opt == "ZUp")
                    {
                        zUp = true;
                    }
                }
            }

            bool bIsBone = false;
            int nLightCount = 0;
            osg::ref_ptr<Options> localOptions = NULL;
            if (options)
                localOptions = options->cloneOptions();
            else
                localOptions = new osgDB::Options();
            localOptions->setObjectCacheHint(osgDB::ReaderWriter::Options::CACHE_IMAGES);

            std::string filePath = osgDB::getFilePath(filename);
            FbxMaterialToOsgStateSet fbxMaterialToOsgStateSet(filePath, localOptions.get(), lightmapTextures);

            std::set<const FbxNode*> fbxSkeletons;
            fbxUtil::findLinkedFbxSkeletonNodes(pNode, fbxSkeletons);

            OsgFbxReader::AuthoringTool authoringTool = OsgFbxReader::UNKNOWN;
            FbxString appName;
            if (FbxDocumentInfo* pDocInfo = pScene->GetDocumentInfo())
            {
                struct ToolName
                {
                    const char* name;
                    OsgFbxReader::AuthoringTool tool;
                };

                ToolName authoringTools[] = {
                    {"OpenSceneGraph", OsgFbxReader::OPENSCENEGRAPH},
                    {"3ds Max", OsgFbxReader::AUTODESK_3DSTUDIO_MAX}
                };

                appName = pDocInfo->LastSaved_ApplicationName.Get();

                for (unsigned int i = 0; i < sizeof(authoringTools) / sizeof(authoringTools[0]); ++i)
                {
                    if (0 ==
#ifdef WIN32
                        _strnicmp
#else
                        strncasecmp
#endif
                        (appName, authoringTools[i].name, strlen(authoringTools[i].name)))
                    {
                        authoringTool = authoringTools[i].tool;
                        break;
                    }
                }
            }


            OsgFbxReader reader(*pSdkManager,
                *pScene,
                fbxMaterialToOsgStateSet,
                fbxSkeletons,
                *localOptions,
                authoringTool,
                lightmapTextures,
                tessellatePolygons,
                currentFilePath);
            
            // Empty texture unit map that will be pass down
            // and filled as needed
            textureUnitMap textureMap;
                        
            ReadResult res = reader.readFbxNode(pNode, bIsBone, nLightCount, textureMap, appName);

            if (res.success())
            {
                fbxMaterialToOsgStateSet.checkInvertTransparency();

                fbxUtil::resolveBindMatrices(*res.getNode(), reader.boneBindMatrices, reader.nodeMap);

                osg::Node* osgNode = res.getNode();
                osgNode->getOrCreateStateSet()->setMode(GL_RESCALE_NORMAL,osg::StateAttribute::ON);
                osgNode->getOrCreateStateSet()->setMode(GL_NORMALIZE,osg::StateAttribute::ON);

                if (reader.pAnimationManager.valid())
                {
                    if (osgNode->getUpdateCallback())
                    {
                        osg::Group* osgGroup = new osg::Group;
                        osgGroup->addChild(osgNode);
                        osgNode = osgGroup;
                    }
                }

                // Unregister the animation here since they will be register in Vantage
                if (reader.pAnimationManager)
                {
                   const osgAnimation::AnimationList& anims = reader.pAnimationManager->getAnimationList();
                   for (size_t i = 0; i < anims.size(); ++i)
                   {
                      osgAnimation::Animation* pAnimation = anims[i].get();
                      reader.pAnimationManager->unregisterAnimation(pAnimation);
                   }
                }

                FbxAxisSystem fbxAxis = pScene->GetGlobalSettings().GetAxisSystem();
                printAxisInfo(fbxAxis);

                //// some reminder: http://www.realtimerendering.com/blog/left-handed-vs-right-handed-world-coordinates/                
                int upSign, frontSign;
                FbxAxisSystem::EUpVector eUp = fbxAxis.GetUpVector(upSign);
                FbxAxisSystem::EFrontVector eFront = fbxAxis.GetFrontVector(frontSign);
                eFront;
                bool bLeftHanded = fbxAxis.GetCoorSystem() == FbxAxisSystem::eLeftHanded;
                float fSign = upSign < 0 ? 1.0f : -1.0f;
                float HorizSign = bLeftHanded ? -1.0f : 1.0f;

                bool refCoordSysChange = false;
                osg::Matrix mat;

                // TODO REMOVE THE CODE BELLOW ONCE FbxAxisSystem::Max.ConvertScene(pScene); is working again
                // We want to put all in Right Hand and Z up
                if (bLeftHanded)
                {
                     // NOT TESTED
                     switch (eUp)
                     {
                     case FbxAxisSystem::eXAxis:
                        mat.set(0, fSign, 0, 0, -fSign, 0, 0, 0, 0, 0, HorizSign, 0, 0, 0, 0, 1); 
                        break;
                     case FbxAxisSystem::eYAxis:
                        mat.set(1, 0, 0, 0, 0, fSign, 0, 0, 0, 0, fSign*HorizSign, 0, 0, 0, 0, 1);
                        break;
                     case FbxAxisSystem::eZAxis:
                        mat.set(1, 0, 0, 0, 0, 0, -fSign * HorizSign, 0, 0, fSign, 0, 0, 0, 0, 0, 1);
                        break;
                     }
                     refCoordSysChange = true;
                }
                else
                {
                     // In this case just change to Z up
                     if (eUp != FbxAxisSystem::eZAxis || fSign != 1.0 || upSign != 1.0)
                     {
                        switch (eUp)
                        {
                        case FbxAxisSystem::eXAxis:
                           mat.set(0, fSign, 0, 0,
                                  -fSign, 0, 0, 0,
                                   0, 0, HorizSign, 0, 
                                   0, 0, 0, 1);
                           refCoordSysChange = true;
                           break;
                        case FbxAxisSystem::eYAxis:
                           mat.set(1, 0, 0, 0, 
                                   0, 0, -fSign * HorizSign, 0, 
                                   0, fSign, 0, 0, 
                                   0, 0, 0, 1);
                           refCoordSysChange = true;
                           break;
                        // Do nothing when Z is up
                        //case FbxAxisSystem::eZAxis:
                        //   mat.set(1, 0, 0, 0, 
                        //           0, fSign, 0, 0, 
                        //           0, 0, fSign*HorizSign, 0, 
                        //           0, 0, 0, 1);
                        //   refCoordSysChange = true;
                        //   break;
                        }
                     }
                }
                
                if (refCoordSysChange)
                {
                   osg::Transform* pTransformTemp = osgNode->asTransform();
                   osg::MatrixTransform* pMatrixTransform = pTransformTemp ?
                      pTransformTemp->asMatrixTransform() : NULL;
                   if (pMatrixTransform)
                   {
                      pMatrixTransform->setMatrix(pMatrixTransform->getMatrix() * mat);
                   }
                   else
                   {
                      pMatrixTransform = new osg::MatrixTransform(mat);
                      pMatrixTransform->setDataVariance(osg::Object::STATIC);
                      if (useFbxRoot && fbxUtil::isBasicRootNode(*osgNode))
                      {
                         // If root node is a simple group, put all FBX elements under the OSG root
                         osg::Group* osgGroup = osgNode->asGroup();
                         for (unsigned int i = 0; i < osgGroup->getNumChildren(); ++i)
                         {
                            pMatrixTransform->addChild(osgGroup->getChild(i));
                         }
                         pMatrixTransform->setName(osgGroup->getName());
                      }
                      else
                      {
                         pMatrixTransform->addChild(osgNode);
                      }
                   }
                   // IMPORTANT: if a transform is added at the top, we need 
                   // to add a group before for instancing to work in Vantage
                   osg::Group* rootGroup = new osg::Group();
                   rootGroup->addChild(pMatrixTransform);
                   osgNode = rootGroup;
                }

                osgNode->setName(filenameInit);

#if 0 // TO DEBUG IN VANTAGE
                std::cout << "File: " << filenameInit << std::endl;
                std::cout << "FBX Model: Begin======" << std::endl;
                makVrv::DtOsgPrintNodeHierarchyVisitor printer;
                osgNode->accept(printer);
                std::cout << "FBX Model: End" << std::endl;
#endif

                // Visitor that goes through a adjust the put matrix of DOF transform
                fbxMatrixTransformVisitor mtVis;
                osgNode->accept(mtVis);
                return osgNode;
            }
        }
    }
    catch (...)
    {
        OSG_WARN << "Exception thrown while importing \"" << filenameInit << '\"' << std::endl;
    }

    return ReadResult::ERROR_IN_READING_FILE;
}

osgDB::ReaderWriter::WriteResult ReaderWriterFBX::writeNode(
    const osg::Node& node,
    const std::string& filename,
    const Options* options) const
{
    try
    {
        std::string ext = osgDB::getLowerCaseFileExtension(filename);
        if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

        osg::ref_ptr<Options> localOptions = options ?
            static_cast<Options*>(options->clone(osg::CopyOp::SHALLOW_COPY)) : new Options;
        localOptions->getDatabasePathList().push_front(osgDB::getFilePath(filename));

        FbxManager* pSdkManager = FbxManager::Create();

        if (!pSdkManager)
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }

        CleanUpFbx cleanUpFbx(pSdkManager);

        pSdkManager->SetIOSettings(FbxIOSettings::Create(pSdkManager, IOSROOT));

        bool useFbxRoot = false;
        bool ascii(false);
        std::string exportVersion;
        if (options)
        {
            std::istringstream iss(options->getOptionString());
            std::string opt;
            while (iss >> opt)
            {
                if (opt == "Embedded")
                {
                    pSdkManager->GetIOSettings()->SetBoolProp(EXP_FBX_EMBEDDED, true);
                }
                else if (opt == "UseFbxRoot")
                {
                    useFbxRoot = true;
                }
                else if (opt == "FBX-ASCII")
                {
                    ascii = true;
                }
                else if (opt == "FBX-ExportVersion")
                {
                    iss >> exportVersion;
                }
            }
        }

        FbxScene* pScene = FbxScene::Create(pSdkManager, "");

        if (options)
        {
            if (options->getPluginData("FBX-AssetUnitMeter"))
            {
                float unit = *static_cast<const float*>(options->getPluginData("FBX-AssetUnitMeter"));
                FbxSystemUnit kFbxSystemUnit(unit*100);
                pScene->GetGlobalSettings().SetSystemUnit(kFbxSystemUnit);
            }
        }

        pluginfbx::WriterNodeVisitor writerNodeVisitor(pScene, pSdkManager, filename,
            options, osgDB::getFilePath(node.getName().empty() ? filename : node.getName()));
        if (useFbxRoot && fbxUtil::isBasicRootNode(node))
        {
            // If root node is a simple group, put all elements under the FBX root
            const osg::Group * osgGroup = node.asGroup();
            for (unsigned int child = 0; child < osgGroup->getNumChildren(); ++child)
            {
                const_cast<osg::Node *>(osgGroup->getChild(child))->accept(writerNodeVisitor);
            }
        }
        else {
            // Normal scene
            const_cast<osg::Node&>(node).accept(writerNodeVisitor);
        }

        FbxDocumentInfo* pDocInfo = pScene->GetDocumentInfo();
        bool needNewDocInfo = pDocInfo != NULL;
        if (needNewDocInfo)
        {
            pDocInfo = FbxDocumentInfo::Create(pSdkManager, "");
        }
        pDocInfo->LastSaved_ApplicationName.Set(FbxString("OpenSceneGraph"));
        pDocInfo->LastSaved_ApplicationVersion.Set(FbxString(osgGetVersion()));
        if (needNewDocInfo)
        {
            pScene->SetDocumentInfo(pDocInfo);
        }

        FbxExporter* lExporter = FbxExporter::Create(pSdkManager, "");
        pScene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::eOpenGL);

        // Ensure the directory exists or else the FBX SDK will fail
        if (!osgDB::makeDirectoryForFile(filename)) {
            OSG_NOTICE << "Can't create directory for file '" << filename << "'. FBX SDK may fail creating the file." << std::endl;
        }

        // The FBX SDK interprets the filename as UTF-8
#ifdef OSG_USE_UTF8_FILENAME
        const std::string& utf8filename(filename);
#else
        std::string utf8filename(osgDB::convertStringFromCurrentCodePageToUTF8(filename));
#endif

        // Output format selection. Here we only handle "recent" FBX, binary or ASCII.
        // pSdkManager->GetIOPluginRegistry()->GetWriterFormatDescription() / GetReaderFormatCount() gives the following list:
        //FBX binary (*.fbx)
        //FBX ascii (*.fbx)
        //FBX encrypted (*.fbx)
        //FBX 6.0 binary (*.fbx)
        //FBX 6.0 ascii (*.fbx)
        //FBX 6.0 encrypted (*.fbx)
        //AutoCAD DXF (*.dxf)
        //Alias OBJ (*.obj)
        //Collada DAE (*.dae)
        //Biovision BVH (*.bvh)
        //Motion Analysis HTR (*.htr)
        //Motion Analysis TRC (*.trc)
        //Acclaim ASF (*.asf)
        int format = ascii ? pSdkManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX ascii (*.fbx)") : -1;        // -1 = Default
        if (!lExporter->Initialize(utf8filename.c_str(), format))
        {
#if FBXSDK_VERSION_MAJOR < 2014
            return std::string(lExporter->GetLastErrorString());
#else
            return std::string(lExporter->GetStatus().GetErrorString());
#endif
        }

        if (!exportVersion.empty() && !lExporter->SetFileExportVersion(FbxString(exportVersion.c_str()), FbxSceneRenamer::eNone)) {
            std::stringstream versionsStr;
            char const * const * versions = lExporter->GetCurrentWritableVersions();
            if (versions) for(; *versions; ++versions) versionsStr << " " << *versions;
            OSG_WARN << "Can't set FBX export version to '" << exportVersion << "'. Using default. Available export versions are:" << versionsStr.str() << std::endl;
        }

        if (!lExporter->Export(pScene))
        {
#if FBXSDK_VERSION_MAJOR < 2014
            return std::string(lExporter->GetLastErrorString());
#else
            return std::string(lExporter->GetStatus().GetErrorString());
#endif
        }

        return WriteResult::FILE_SAVED;
    }
    catch (const std::string& s)
    {
        return s;
    }
    catch (const char* s)
    {
        return std::string(s);
    }
    catch (...)
    {
    }

    return WriteResult::ERROR_IN_WRITING_FILE;
}

///////////////////////////////////////////////////////////////////////////
// Add ourself to the Registry to instantiate the reader/writer.

REGISTER_OSGPLUGIN(fbx, ReaderWriterFBX)
