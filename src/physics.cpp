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
        return JPH::BroadPhaseLayer(objectLayer);
    }

    virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return "Unimplemented";
    }
};

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
