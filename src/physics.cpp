// Responsible for definitions used by the physics system, and also a
// conversion from our coordinate system to Jolt's.

// TODO(marvin): Should there be any Jolt definitions like these in engine space, should they only exist in user space?


/* From the Jolt 5.3.0 documentation:
  
   "A standard setup would be to have at least 2 broad phase layers:
   One for all static bodies (which is infrequently updated but is
   expensive to update since it usually contains most bodies) and one
   for all dynamic bodies (which is updated every simulation step but
   cheaper to update since it contains fewer objects)."
*/

// NOTE(marvin): JPH::ObjectLayer is a typedef for an integer, whereas JPH::BroadPhaseLayer is a class that needs to be constructed, hence the `= ...` vs `(...)`.

namespace Layer
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr u32 NUM_LAYERS = 2;
};

namespace BroadPhaseLayer
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr u32 NUM_LAYERS(2);
};

class SklBroadPhaseLayer final : public JPH::BroadPhaseLayerInterface
{
public:
    virtual u32 GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayer::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer objectLayer) const override
    {
        Assert(objectLayer < this->GetNumBroadPhaseLayers);
        // NOTE(marvin): Layer and BroadPhaseLayer maps 1:1, which is why we can do this.
        return JPH::BroadPhaseLayer(objectLayer);
    }

    virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return "SKLBroadPhaseLayer";
    }
};

// NOTE(marvin): See https://jrouwe.github.io/JoltPhysicsDocs/5.3.0/index.html#collision-detection

class SklObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
    {
        switch (layer1)
        {
            case Layer::NON_MOVING:
            {
                return layer2 == BroadPhaseLayer::MOVING;
            }
            case Layer::MOVING:
            {
                return true;
            }
            default:
            {
                return false;
            }
            
        }
    }
};

class SklObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        switch (layer1)
        {
            case Layer::NON_MOVING:
            {
                return layer2 == Layer::MOVING;
            }
            case Layer::MOVING:
            {
                return true;
            }
            default:
            {
                return false;
            }
            
        }
    }
};



inline JPH::Vec3 OurToJoltCoordinateSystem(glm::vec3 ourVec3)
{
    f32 rx = -ourVec3.y;
    f32 ry = ourVec3.z;
    f32 rz = ourVec3.x;
    JPH::Vec3 result{rx, ry, rz};
    return result;
}

inline glm::vec3 JoltToOurCoordinateSystem(JPH::Vec3 joltVec3)
{
    f32 rx = joltVec3.GetZ();
    f32 ry = -joltVec3.GetX();
    f32 rz = joltVec3.GetY();
    glm::vec3 result{rx, ry, rz};
    return result;
}
