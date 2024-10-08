#include "phys_main.h"

#include <cstdarg>
#include <thread>

using namespace JPH::literals;

static void 
trace_impl(const char* inFMT, ...) {
	va_list list;
	va_start(list, inFMT);

	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	fprintf(stderr, "[Physics] %s\n", buffer);
}

#ifdef JPH_ENABLE_ASSERTS

static bool 
assert_failed_impl(const char* inExpr, const char* inMsg,
	const char* inFile, uint inLine) {
	fprintf(stderr, "[Physics] %s:%d: (%s) %s\n",
		inFile,
		inLine,
		inExpr,
		(inMsg != nullptr ? inMsg : "")
	);
	return true;
}

#endif /* JPH_ENABLE_ASSERTS */


void 
PhysicsContext::init(VulkanEngine* pVulkanEngine) {
	RegisterDefaultAllocator();

	Trace = trace_impl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = assert_failed_impl);

	Factory::sInstance = new Factory();

	RegisterTypes();

	_pTempAllocator = new TempAllocatorImpl(TEMP_ALLOC_SIZE);
	_pJobSystem = new JobSystemThreadPool(cMaxPhysicsJobs,
		cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	_physicsSystem.Init(MAX_BODIES, MAX_BODY_MUTEXES, MAX_BODY_PAIRS,
		MAX_CONTACT_CONSTRAINTS, _broadPhaseLayerInterface, 
		_objectVsBroadphaseLayerFilter, _objectVsObjectLayerFilter);

	_physicsSystem.SetBodyActivationListener(&_bodyActivationListener);

	_physicsSystem.SetContactListener(&_contactListener);
	
	_pDebugRenderer = new DebugRendererSimpleImpl(pVulkanEngine);
}

void 
PhysicsContext::deinit() {
	BodyInterface& bodyInterface = _physicsSystem.GetBodyInterface();

	bodyInterface.RemoveBodies(_bodyIDs.data(), _bodyIDs.size());
	bodyInterface.DestroyBodies(_bodyIDs.data(), _bodyIDs.size());

	UnregisterTypes();

	delete _pDebugRenderer;
	delete _pTempAllocator;
	delete _pJobSystem;

	delete Factory::sInstance;
	Factory::sInstance = nullptr;
}

BodyID
PhysicsContext::add_box(Transform transform, Vec3 extent) {
	BodyInterface& bodyInterface = _physicsSystem.GetBodyInterface();

	// Normalize quats before use in JPH
	glm::quat q = glm::normalize(transform.rotation);

	BodyCreationSettings boxSettings(
		new BoxShape(extent),
		RVec3(transform.position.x, transform.position.y, transform.position.z),
		Quat(q.x, q.y, q.z, q.w),
		EMotionType::Kinematic,
		Layers::MOVING
	);

	BodyID boxID = bodyInterface.CreateAndAddBody(boxSettings,
		EActivation::Activate);

	_bodyIDs.push_back(boxID);
	return boxID;
}

void 
PhysicsContext::update(float dt) {
	_accumulator += dt;
	
	BodyInterface& bodyInterface = _physicsSystem.GetBodyInterface();
	uint step = 0;
	++step;

	RVec3 position = bodyInterface.GetCenterOfMassPosition(_testSphereID);
	Vec3 velocity = bodyInterface.GetLinearVelocity(_testSphereID);

	const int cCollisionSteps = 1;

	BodyManager::DrawSettings drawSettings;
	drawSettings.mDrawShapeWireframe = true;
	_physicsSystem.DrawBodies(drawSettings, _pDebugRenderer);

	while (_accumulator >= TIME_STEP) {
		_physicsSystem.Update(TIME_STEP, cCollisionSteps,
			_pTempAllocator, _pJobSystem);
		_accumulator -= TIME_STEP;
	}
}