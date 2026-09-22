#include <skl_math_utils.h>

#include <random>
#include <iostream>
#include <vector>
#include <random>
#include <numbers>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

glm::vec3 Transform3D::GetLocalPosition()
{
    return this->position;
}
void Transform3D::SetLocalPosition(glm::vec3 newPos)
{
    this->position = newPos;
    MarkDirty();
}
void Transform3D::AddLocalPosition(glm::vec3 offset)
{
    this->position += offset;
    MarkDirty();
}
glm::vec3 Transform3D::GetLocalRotation()
{
    return rotation;
}
void Transform3D::SetLocalRotation(glm::vec3 newRot)
{
    this->rotation = newRot;
    MarkDirty();
}
void Transform3D::AddLocalRotation(glm::vec3 offset)
{
    this->rotation += offset;
    MarkDirty();
}
glm::vec3 Transform3D::GetLocalScale()
{
    return this->scale;
}
void Transform3D::SetLocalScale(glm::vec3 newScale)
{
    this->scale = newScale;
    MarkDirty();
}

void Transform3D::MarkDirty()
{
    if (!this->dirty)
    {
        this->dirty = true;
        for (Transform3D *child : children)
        {
            child->MarkDirty();
        }
    }
}

glm::mat4 Transform3D::GetWorldTransform()
{
    if (this->dirty)
    {
        glm::quat aroundX = glm::angleAxis(glm::radians(this->rotation.x), glm::vec3(1.0, 0.0, 0.0));
        glm::quat aroundY = glm::angleAxis(glm::radians(this->rotation.y), glm::vec3(0.0, 1.0, 0.0));
        glm::quat aroundZ = glm::angleAxis(glm::radians(this->rotation.z), glm::vec3(0.0, 0.0, 1.0));
        glm::mat4 rotationMat = glm::mat4_cast(aroundZ * aroundY * aroundX);

        this->dirty = false;

        this->worldTransform = glm::scale(glm::translate(glm::mat4(1.0f), this->position) * rotationMat, this->scale);

        if (parent != nullptr)
        {
            this->worldTransform = this->parent->GetWorldTransform() * this->worldTransform;
        }
    }

    return this->worldTransform;
}

glm::vec3 Transform3D::GetWorldPosition()
{
    return GetWorldTransform()[3];
}

glm::vec3 Transform3D::GetForwardVector()
{
    return GetWorldTransform()[0];
}

glm::vec3 Transform3D::GetRightVector()
{
    return GetWorldTransform()[1];
}

glm::vec3 Transform3D::GetUpVector()
{
    return GetWorldTransform()[2];
}

glm::mat4 MakeViewMatrix(glm::vec3 forward, glm::vec3 right, glm::vec3 up, glm::vec3 position)
{
    glm::mat4 view = {};
    view[0] = {right.x, up.x, forward.x, 0};
    view[1] = {right.y, up.y, forward.y, 0};
    view[2] = {right.z, up.z, forward.z, 0};
    view[3] = {-glm::dot(right, position),
               -glm::dot(up, position),
               -glm::dot(forward, position),
               1};

    return view;
}

glm::mat4 Transform3D::GetViewMatrix()
{
    glm::vec3 forward = GetForwardVector();
    glm::vec3 right = GetRightVector();
    glm::vec3 up = GetUpVector();

    return MakeViewMatrix(forward, right, up, GetWorldTransform() * glm::vec4(0, 0, 0, 1));
}

void Transform3D::GetPointViews(glm::mat4 *views)
{
    glm::vec3 forward = {1, 0, 0};
    glm::vec3 right = {0, 1, 0};
    glm::vec3 up = {0, 0, 1};

    glm::vec3 worldPosition = GetWorldTransform() * glm::vec4(0, 0, 0, 1);

    views[0] = MakeViewMatrix(forward, -up, right, worldPosition);
    views[1] = MakeViewMatrix(-forward, up, right, worldPosition);
    views[2] = MakeViewMatrix(right, forward, -up, worldPosition);
    views[3] = MakeViewMatrix(-right, forward, up, worldPosition);
    views[4] = MakeViewMatrix(up, forward, right, worldPosition);
    views[5] = MakeViewMatrix(-up, -forward, right, worldPosition);
}

void Transform3D::SetParent(Transform3D *newParent)
{
    if (this->parent != nullptr)
    {
        this->parent->children.erase(this);
    }

    this->parent = newParent;
    if (newParent != nullptr)
    {
        newParent->children.insert(this);
    }
    MarkDirty();
}

Transform3D *Transform3D::GetParent()
{
    return this->parent;
}

Transform3D::~Transform3D()
{
    if (this->parent != nullptr)
    {
        this->parent->children.erase(this);
    }

    for (Transform3D *child : children)
    {
        child->parent = nullptr;
    }
}

// Generates a random float in the inclusive range of the two given
// floats.
f32 RandInBetween(f32 LO, f32 HI)
{
    // From https://stackoverflow.com/questions/686353/random-float-number-generation
    return LO + static_cast<f32>(rand()) / (static_cast<f32>(RAND_MAX / (HI - LO)));
}

u32 RandInt(u32 min, u32 max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<u32> distribution(min, max);
    return distribution(gen);
}

glm::vec3 GetArbitraryOrthogonal(const glm::vec3& vec) {
  if (std::abs(vec.x) < std::abs(vec.y) && std::abs(vec.x) < std::abs(vec.z)) {
    return glm::normalize(glm::cross(vec, glm::vec3(1, 0, 0)));
  } else if (abs(vec.y) < abs(vec.z)) {
    return glm::normalize(glm::cross(vec, glm::vec3(0, 1, 0)));
  } else {
    return glm::normalize(glm::cross(vec, glm::vec3(0, 0, 1)));
  }
}

glm::mat4x4 GetMatrixSpace(const glm::vec3& forward, const glm::vec3& up, const glm::vec3& left) {
  return glm::mat4x4(
    glm::vec4(left, 0.0f),
    glm::vec4(up, 0.0f),
    glm::vec4(forward, 0.0f),
    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
  );
}

void ScaleMatrix(glm::mat4x4& mat, const glm::vec3& scaleVec) {
    mat[0] *= scaleVec[0];
    mat[1] *= scaleVec[1];
    mat[2] *= scaleVec[2];
}

glm::vec3 GetScaleFromView(const glm::mat4x4& viewMat) {
    return {
        glm::length(glm::vec3(viewMat[0].x,viewMat[1].x,viewMat[2].x)),
        glm::length(glm::vec3(viewMat[0].y,viewMat[1].y,viewMat[2].y)),
        glm::length(glm::vec3(viewMat[0].z,viewMat[1].z,viewMat[2].z))
    };
}

glm::vec3 GetForwardVecFromView(const glm::mat4x4& viewMat) {
    return glm::normalize(glm::vec3(
        viewMat[0].z,
        viewMat[1].z,
        viewMat[2].z
    ));
}

glm::vec3 GetWorldTranslateFromView(const glm::mat4x4& viewMat) {
    return -glm::transpose(glm::mat3x3(GetRotMat(viewMat))) * glm::vec3(viewMat[3]);
};

glm::mat4x4 GetRotMat(const glm::mat4x4& mat) {
    glm::mat4x4 cpy = mat;
    cpy[0] = glm::vec4(glm::normalize(glm::vec3(cpy[0])), 0.0f);
    cpy[1] = glm::vec4(glm::normalize(glm::vec3(cpy[1])), 0.0f);
    cpy[2] = glm::vec4(glm::normalize(glm::vec3(cpy[2])), 0.0f);
    cpy[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    return cpy;
}


std::vector<glm::vec4> GetFrustumCorners(const glm::mat4& proj, const glm::mat4& view)
{
    glm::mat4 inverse = glm::inverse(proj * view);

    std::vector<glm::vec4> frustumCorners;
    for (u32 x = 0; x < 2; ++x)
    {
        for (u32 y = 0; y < 2; ++y)
        {
            for (u32 z = 0; z < 2; ++z)
            {
                const glm::vec4 pt =
                        inverse * glm::vec4(
                                2.0f * x - 1.0f,
                                2.0f * y - 1.0f,
                                z,
                                1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}

struct coordKey {
    f32 sampleWeight;
    glm::vec2 coord;
};

static inline f32 sampleWeight(f32 twoRMax, f32 dist) {
    f32 base = (1.0f - (dist/twoRMax));
    base *= base;
    base *= base;
    base *= base;
    return base;
}
 
std::vector<glm::vec2> BuildPoissonDisk(f32 diskRadius, u32 diskCount) {
    ASSERT_PRINT(diskCount != 0, "Poisson disk of 0 cannot be built");

    // Arbitrarily sets a oversampling rate of about 4* before sample exclusion
    u32 adjustedSampleSize = diskCount * 4;
    u32 thetaDivisions = sqrt(adjustedSampleSize);
    u32 rangeDivisions = adjustedSampleSize / thetaDivisions;

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<double> dis(0.0, 1.0);

    std::vector<glm::vec2> coords;
    coords.reserve(thetaDivisions * rangeDivisions);

    // Generates a random point in each division
    f32 thetaStep = (2 * SKL_PI) / static_cast<f32>(thetaDivisions);
    f32 sqRadiusStep = 1 / static_cast<f32>(rangeDivisions);
    for (u32 t = 0 ; t < thetaDivisions ; t += 1) {
        f32 minTheta = t * thetaStep;
        for (u32 d = 0 ; d < rangeDivisions ; d += 1) {
            f32 sqMinRange = sqRadiusStep * d;

            f32 thetaRd = dis(gen);
            f32 sqRadialRd = dis(gen);

            f32 radial = sqMinRange + (sqRadialRd * sqRadiusStep);


            coords.emplace_back((minTheta + thetaRd * thetaStep), sqrt(radial) * diskRadius);
        }
    }

    // Converts coords from polar to cartesian coords
    for (glm::vec2& coord : coords) {
        f32 theta = coord.x;
        f32 radius = coord.y;

        coord.x = radius * cos(theta);
        coord.y = radius * sin(theta);
    }

    // Applies sample exclusion to calculate index        
    // Calculate initial scores
    f32 rMax = sqrt((SKL_PI * diskRadius * diskRadius) / (2 * sqrt(3) * diskCount));
    f32 twoRMax = 2 * rMax;

    std::vector<coordKey> coordCopy;
    coordCopy.reserve(coords.size());

    for (u32 i = 0 ; i < coords.size() ; i += 1) {
        coordCopy.emplace_back(0, coords[i]);
    }

    for (u32 i = 0 ; i < coords.size() ; i += 1) {
        for (u32 z = i + 1 ; z < coords.size() ; z += 1) {
            f32 dist = glm::length(coordCopy[i].coord - coordCopy[z].coord);
            if (dist <= twoRMax) {
                f32 scoreAdd = sampleWeight(twoRMax, dist);
                coordCopy[i].sampleWeight += scoreAdd;
                coordCopy[z].sampleWeight += scoreAdd;
            }
        }
    }

    // Repeatedly find most bundled points and remove them from coords
    while (coordCopy.size() > diskCount) {
        std::vector<coordKey>::iterator maxSampleWeightCoords = 
            std::max_element(
                coordCopy.begin(), 
                coordCopy.end(), 
                [](const coordKey& lhs, const coordKey& rhs) {return lhs.sampleWeight < rhs.sampleWeight; });

        glm::vec2 erasedCoord = maxSampleWeightCoords->coord;

        std::swap(*maxSampleWeightCoords, coordCopy.back());
        coordCopy.pop_back();

        for (u32 z = 0 ; z < coordCopy.size() ; z += 1) {
            f32 dist = glm::length(coordCopy[z].coord - erasedCoord);
            if (dist <= twoRMax) {
                f32 scoreAdd = sampleWeight(twoRMax, dist);
                coordCopy[z].sampleWeight -= scoreAdd;
            }
        }
    }

    std::vector<glm::vec2> retCoords;
    retCoords.reserve(diskCount);

    for (u32 i = 0 ; i < diskCount ; i++) {
        retCoords.push_back(coordCopy[i].coord);
    }

    return retCoords;
}

std::vector<glm::vec2> BuildVogelDisk(f32 diskRadius, u32 diskCount) {
    ASSERT_PRINT(diskCount != 0, "Vogel disk of 0 cannot be built");
    if (diskCount == 1) {
        return { glm::vec2(0.0f, 0.0f) };
    }
    std::vector<glm::vec2> ret;
    ret.reserve(diskCount);

    f32 goldenAngle = 2.3999632297;
    for (u32 i = 0 ; i < diskCount ; i += 1) {
        f32 radius = diskRadius * sqrt(static_cast<f32>(i + 0.5f)/static_cast<f32>(diskCount));
        f32 theta = goldenAngle * i;

        ret.push_back(radius * glm::vec2(cos(theta), sin(theta)));
    }

    return ret;
}