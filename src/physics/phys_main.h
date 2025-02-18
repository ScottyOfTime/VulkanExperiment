#ifndef PHYS_MAIN_H
#define PHYS_MAIN_H

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "phys_debug.h"

using namespace JPH;

#define TEMP_ALLOC_SIZE				10 * 1024 * 1024

constexpr uint MAX_BODIES = 1024;
constexpr uint MAX_BODY_MUTEXES = 0;
constexpr uint MAX_BODY_PAIRS = 1024;
constexpr uint MAX_CONTACT_CONSTRAINTS = 1024;

constexpr float		TIME_STEP = 1.f / 60.f;

// An example contact listener
class ContactListenerImpl : public ContactListener
{
public:
	// See: ContactListener
	virtual ValidateResult	OnContactValidate(
		const Body& inBody1,
		const Body& inBody2, RVec3Arg inBaseOffset,
		const CollideShapeResult& inCollisionResult) override
	{
		fprintf(stderr, "[Physics] Contact validate callback\n");

		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void OnContactAdded(
		const Body& inBody1,
		const Body& inBody2,
		const ContactManifold& inManifold,
		ContactSettings& ioSettings) override
	{
		fprintf(stderr, "[Physics] A contact was added\n");
	}

	virtual void OnContactPersisted(
		const Body& inBody1,
		const Body& inBody2,
		const ContactManifold& inManifold,
		ContactSettings& ioSettings) override
	{
		fprintf(stderr, "[Physics] A contact was persisted\n");
	}

	virtual void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
	{
		fprintf(stderr, "[Physics] A contact was removed\n");
	}
};

// An example activation listener
class BodyActivationListenerImpl : public BodyActivationListener
{
public:
	virtual void OnBodyActivated(
		const BodyID& inBodyID,
		uint64 inBodyUserData) override
	{
		fprintf(stderr, "[Physics] A body got activated\n");
	}

	virtual void OnBodyDeactivated(
		const BodyID& inBodyID,
		uint64 inBodyUserData) override
	{
		fprintf(stderr, "[Physics] A body went to sleep\n");
	}
};

namespace Layers
{
	static constexpr ObjectLayer NON_MOVING = 0;
	static constexpr ObjectLayer MOVING = 1;
	static constexpr ObjectLayer NUM_LAYERS = 2;
};

// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
	virtual bool ShouldCollide(ObjectLayer inObj1, ObjectLayer inObj2) const override {
		switch (inObj1) {
		case Layers::NON_MOVING:
			return inObj2 == Layers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// Broadphase layers definitions and mappings
namespace BroadPhaseLayers
{
	static constexpr BroadPhaseLayer NON_MOVING(0);
	static constexpr BroadPhaseLayer MOVING(1);
	static constexpr uint NUM_LAYERS(2);
}

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
	BPLayerInterfaceImpl() {
		_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint GetNumBroadPhaseLayers() const override {
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return _objectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
		switch ((BroadPhaseLayer::Type)inLayer) {
		case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
			return "NON_MOVING";
		case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
			return "MOVING";
		default:
			JPH_ASSERT(false);
			return "INVALID";
		}
	}

#endif /* JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED */

private:
	BroadPhaseLayer _objectToBroadPhase[Layers::NUM_LAYERS];
};

// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
		switch (inLayer1) {
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};


class PhysicsContext {
public:
	void								init(VulkanEngine* pVulkanEngine);
	void								deinit();

	Character*							create_character(Vec3 position, float radius, float height);

	BodyID								add_box(Transform transform, Vec3 extent);

	PhysicsSystem* get_physics_system() {
		return &_physicsSystem;
	}

	void								set_debug_flags(PhysicsDebugFlags flags);
	void								clear_debug_flags(PhysicsDebugFlags flags);

	void								update(float dt);
	
	// This should also probably be private but need direct access for ImGui
	// drawing in the edit game state
	PhysicsDebugFlags					_debugFlags;
private:
	BodyActivationListenerImpl			_bodyActivationListener;
	ContactListenerImpl					_contactListener;

	BPLayerInterfaceImpl				_broadPhaseLayerInterface;
	ObjectVsBroadPhaseLayerFilterImpl	_objectVsBroadphaseLayerFilter;
	ObjectLayerPairFilterImpl			_objectVsObjectLayerFilter;

	TempAllocatorImpl*					_pTempAllocator;
	JobSystemThreadPool*				_pJobSystem;
	DebugRendererSimpleImpl*			_pDebugRenderer;

	PhysicsSystem						_physicsSystem;
	BodyIDVector						_bodyIDs;
	std::vector<Character*>				_characters;

	float								_accumulator = 0.f;
};



#endif /* PHYS_MAIN_H */