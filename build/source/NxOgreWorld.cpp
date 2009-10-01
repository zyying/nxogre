/** 
    
    This file is part of NxOgre.
    
    Copyright (c) 2009 Robin Southern, http://www.nxogre.org
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
    
*/

                                                                                       

#include "NxOgreStable.h"
#include "NxOgreWorld.h"
#include "NxOgreWorldDescription.h"
#include "NxOgreWorldPrototype.h"
#include "NxOgrePrototypeFunctions.h"
#include "NxOgreErrorStream.h"
#include "NxOgreTimeController.h"
#include "NxOgreResourceSystem.h"
#include "NxOgreMeshManager.h"
#include "NxOgreHeightFieldManager.h"
#include "NxOgreReason.h"
#include "NxOgrePhysXOutputStream.h"
#include "NxOgrePhysXUserAllocator.h"
#include "NxOgreScene.h"
#include "NxOgreSceneDescription.h"
#include "NxOgreCallback.h"
#include "NxOgreVisualDebugger.h"
#include "NxOgreRemoteDebugger.h"
#include "NxOgreUtil.h"

#include "NxPhysics.h"
#include "NxCooking.h"

#if NxOgreMemoryDebugger == 1
extern void setNxOgreCreationStatus(int);
#endif
                                                                                       

namespace NxOgre
{

                                                                                       

World* World::sWorldInstance = 0;
NxCookingInterface* World::mCookingInterface = 0;

                                                                                       

void World::precreateSingletons(void)
{
 
 mCookingInterface = NxGetCookingLib(NX_PHYSICS_SDK_VERSION);
 mCookingInterface->NxInitCooking();
 
 if (ErrorStream::getSingleton() == 0)
  NxOgre_New(ErrorStream)();
 if (ResourceSystem::getSingleton() == 0)
  NxOgre_New(ResourceSystem)();
 if (MeshManager::getSingleton() == 0)
  NxOgre_New(MeshManager)();
 if (HeightFieldManager::getSingleton() == 0)
  NxOgre_New(HeightFieldManager)();
 if (TimeController::getSingleton() == 0)
  NxOgre_New(TimeController)();
 
#ifdef NXOGRE_OPTIONS_USE_LOG
 {
  Resource* resource = ResourceSystem::getSingleton()->open(Path("file://NxOgre.log"), Enums::ResourceAccess_WriteOnly);
  if (resource)
   ErrorStream::getSingleton()->setLogResource(resource);
 }
#endif

}

void World::destroySingletons(void)
{
 
 if (ErrorStream::getSingleton() != 0)
 {
  ErrorStream* error_stream = ErrorStream::getSingleton();
  NxOgre_Delete(error_stream);
  ErrorStream::sSingleton = 0;
 }

 if (TimeController::getSingleton() != 0)
 {
  TimeController* time_controller = TimeController::getSingleton();
  NxOgre_Delete(time_controller);
  TimeController::sSingleton = 0;
 }

 if (ResourceSystem::getSingleton() != 0)
 {
  ResourceSystem* resource_system = ResourceSystem::getSingleton();
  NxOgre_Delete(resource_system);
  ResourceSystem::sSingleton = 0;
 }
 
 if (MeshManager::getSingleton() != 0)
 {
  MeshManager* mesh_manager = MeshManager::getSingleton();
  NxOgre_Delete(mesh_manager);
  MeshManager::sSingleton = 0;
 }
 
 if (HeightFieldManager::getSingleton() != 0)
 {
  HeightFieldManager* hf_manager = HeightFieldManager::getSingleton();
  NxOgre_Delete(hf_manager);
  HeightFieldManager::sSingleton = 0;
 }
 
 mCookingInterface->NxCloseCooking();
 
}

World* World::createWorld(const WorldDescription&)
{
 if (World::sWorldInstance != 0)
  return 0;
 
 precreateSingletons();
 WorldPrototype* prototype = NxOgre_New(WorldPrototype)();
 World::sWorldInstance = NxOgre_New(World)(prototype);
 NxOgre_Delete(prototype);
 return World::sWorldInstance;
}

World* World::createWorld(WorldPrototype* prototype)
{
 if (World::sWorldInstance != 0)
  return 0;
 
  precreateSingletons();
  World::sWorldInstance = NxOgre_New(World)(prototype);
 
 return World::sWorldInstance;
}

void World::destroyWorld(bool also_destroy_singletons)
{
 if (World::sWorldInstance == 0)
  return;
 
 NxOgre_Delete(World::sWorldInstance);

 if (also_destroy_singletons)
  destroySingletons();

}

World::World(NxOgre::WorldPrototype* prototype)
: mSDK(0), mDeadSDK(false), mPhysXOutputStream(0), mPhysXUserAllocator(0), mNullCallback(0)
{
 
#if NxOgreMemoryDebugger == 1
 setNxOgreCreationStatus(2);
#endif
 mPhysXOutputStream = NxOgre_New(PhysXOutputStream)();
 mPhysXUserAllocator = NxOgre_New(PhysXUserAllocator)();
  
 NxPhysicsSDKDesc sdk_description;
 ::NxOgre::Functions::PrototypeFunctions::WorldPrototypeToNxPhysicsSDKDesc(prototype, sdk_description);
  
 NxSDKCreateError errorCode = NXCE_NO_ERROR;
 mSDK = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION, mPhysXUserAllocator, mPhysXOutputStream, sdk_description, &errorCode); 
  
 if (errorCode != NXCE_NO_ERROR || mSDK == 0)
 {
  SharedStringStream message(SharedStringStream::_LARGE); 
 
  message << "An error occured creating the PhysicSDK.\nThe reason(s) and cause(s) could be:\n\n" << Reason::why(errorCode);
  
  if (mSDK == 0)
   message << "- NxPhysicsSDK pointer is null\n";
  
  if (errorCode == NXCE_DESCRIPTOR_INVALID)
   message << Reason::whyAsStream(sdk_description).get();
  
  
  NxOgre_ThrowError(message.get());
  mDeadSDK = true;
  return;
 }

 mNullCallback = NxOgre_New(Callback)();

 mVisualDebugger = new VisualDebugger(this);
 mRemoteDebugger = new RemoteDebugger(this);

}

World::~World(void)
{
 
 NxOgre_Delete(mRemoteDebugger);
 NxOgre_Delete(mVisualDebugger);
 
 mScenes.clear();
 
 if (!mDeadSDK && mSDK)
  mSDK->release();
 
 NxOgre_Delete(mNullCallback);
 NxOgre_Delete(mPhysXOutputStream);
 NxOgre_Delete(mPhysXUserAllocator);
 
#if NxOgreMemoryDebugger == 1
 setNxOgreCreationStatus(1);
#endif

}

bool World::isDead(void) const
{
 return mDeadSDK;
}

Scene* World::createScene(const NxOgre::SceneDescription& description)
{
 Scene* scene = NxOgre_New(Scene)(description, mSDK);
 StringHash hash = scene->getNameHash();
 mScenes.insert(hash, scene);
 return scene;
}

void World::destroyScene(Scene* scene)
{
 if (scene == 0)
  return;
 
 mScenes.erase(scene->getNameHash());
 
}

bool World::hasHardware(void) const
{
 if (mDeadSDK)
  return false;
 return mSDK->getHWVersion() != NX_HW_VERSION_NONE;
}

World* World::getWorld()
{
 return sWorldInstance;
}

Callback* World::getNullCallback()
{
 return mNullCallback;
}

VisualDebugger* World::getVisualDebugger()
{
 return mVisualDebugger;
}

RemoteDebugger* World::getRemoteDebugger()
{
 return mRemoteDebugger;
}

World::SceneIterator World::getScenes()
{
 return SceneIterator(mScenes);
}

NxPhysicsSDK* World::getPhysXSDK(void)
{
 return mSDK;
}

NxCookingInterface* World::getPhysXCookingInterface(void)
{
 return mCookingInterface;
}

                                                                                       

} // namespace NxOgre

                                                                                       
