#include <demo_systems.h>
#include <meta_definitions.h>
#include <engine_components.h>
#include <game_components.h>
#include <utils.h>
#include <scene_view.h>

MAKE_SYSTEM_MANUAL_VTABLE(RectCollision2DSystem);

MAKE_SYSTEM_MANUAL_VTABLE(PongMovement2DSystem);
MAKE_SYSTEM_MANUAL_VTABLE(PongBall2DSystem);

MAKE_SYSTEM_MANUAL_VTABLE(FightBall2DSystem);
MAKE_SYSTEM_MANUAL_VTABLE(Fighter2DSystem);

RectCollision2DSystem::RectCollision2DSystem() : SYSTEM_SUPER(RectCollision2DSystem) {}

PongMovement2DSystem::PongMovement2DSystem() : SYSTEM_SUPER(PongMovement2DSystem) {}
PongBall2DSystem::PongBall2DSystem() : SYSTEM_SUPER(PongBall2DSystem) {}

FightBall2DSystem::FightBall2DSystem() : SYSTEM_SUPER(FightBall2DSystem) {}
Fighter2DSystem::Fighter2DSystem() : SYSTEM_SUPER(Fighter2DSystem) {}

inline glm::vec4 getBounds(CollisionBox2D *box , Transform3D *t) 
{
    glm::vec3 pos = t->GetLocalPosition();
    glm::vec3 scale = t->GetLocalScale();

    f32 dimWidth = scale.x * box->relativeWidth;
    f32 dimHeight = scale.y * box->relativeHeight;
    f32 left = pos.x - (dimWidth / 2.0f);
    f32 bot = pos.y - (dimHeight / 2.0f);
    f32 right = pos.x + (dimWidth / 2.0f);
    f32 top = pos.y + (dimHeight / 2.0f); 

    return {left, bot, right, top};
}
inline bool collide(glm::vec4 bounds1, glm::vec4 bounds2) 
{
    return bounds1.x <= bounds2.z && 
        bounds1.z >= bounds2.x &&  
        bounds1.y <= bounds2.w &&  
        bounds1.w >= bounds2.y; 
}

// Assumes only one collision
SYSTEM_ON_UPDATE(RectCollision2DSystem)
{
    for (EntityID ent: SceneView<CollisionBox2D>(*scene))
    {
        CollisionBox2D *box = scene->Get<CollisionBox2D>(ent);
        box->colTag = "empty";
    }

    for (EntityID ent: SceneView<CollisionBox2D, Transform3D>(*scene))
    {
        CollisionBox2D *box = scene->Get<CollisionBox2D>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);
        
        glm::vec4 baseBounds = getBounds(box, t);

        for (EntityID innerEnt: SceneView<CollisionBox2D, Transform3D>(*scene)) {
            if (innerEnt == ent) {
                continue;
            }

            CollisionBox2D *innerBox = scene->Get<CollisionBox2D>(innerEnt);
            Transform3D *innerT = scene->Get<Transform3D>(innerEnt);

            glm::vec4 innerBounds = getBounds(innerBox, innerT);

            bool isColliding = baseBounds.z > innerBounds.x && 
                baseBounds.x < innerBounds.z && 
                baseBounds.w > innerBounds.y && 
                baseBounds.y < innerBounds.w; 

            if (isColliding) {
                box->colTag = innerBox->thisTag;
                innerBox->colTag = box->thisTag;
                break;
            }
        }
    }
}

inline bool IsButtonDown(GameInput *input, const std::string &key)
{
    if (OnPress(input, key) || OnHold(input, key)) return true;
    std::string upper = key;
    for (char &c : upper) c = toupper(c);
    if (OnPress(input, upper) || OnHold(input, upper)) return true;
    std::string lower = key;
    for (char &c : lower) c = tolower(c);
    if (OnPress(input, lower) || OnHold(input, lower)) return true;
    return false;
}
SYSTEM_ON_UPDATE(PongMovement2DSystem)
{
    for (EntityID ent: SceneView<PongMovement2D, Transform3D>(*scene))
    {
        PongMovement2D *f = scene->Get<PongMovement2D>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);

        bool pressUp = IsButtonDown(input, f->upButton);
        bool pressDown = IsButtonDown(input, f->downButton);
        if (!pressUp && pressDown) {
            t->AddLocalPosition({0, f->moveSpeed * deltaTime, 0});
            glm::vec3 pos = t->GetLocalPosition();
            pos.y = std::min(pos.y, f->topBoundY);
            t->SetLocalPosition(pos);
        }
        else if (!pressDown && pressUp) {
            t->AddLocalPosition({0, -f->moveSpeed * deltaTime, 0});
            glm::vec3 pos = t->GetLocalPosition();
            pos.y = std::max(pos.y, f->bottomBoundY);
            t->SetLocalPosition(pos);
        }
    }
}
SYSTEM_ON_UPDATE(PongBall2DSystem)
{
    for (EntityID ent: SceneView<PongBall2D, CollisionBox2D, Transform3D>(*scene))
    {
        PongBall2D *ball = scene->Get<PongBall2D>(ent);
        CollisionBox2D *box = scene->Get<CollisionBox2D>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);

        glm::vec3 pos = t->GetLocalPosition();
        
        const glm::vec3 previous = pos;
        pos.x += ball->xVel * deltaTime;
        pos.y += ball->yVel * deltaTime;

        // Missing a paddle starts a new rally at the center.
        if (pos.x > ball->rightBound || pos.x < ball->leftBound) {
            t->SetLocalPosition({0.0f, 0.0f, pos.z});
            ball->xVel = -ball->xVel;
            continue;
        }
        if (pos.y > ball->upBound) {
            pos.y = ball->upBound;
            ball->yVel = -ball->yVel;
        }
        if (pos.y < ball->downBound) {
            pos.y = ball->downBound;
            ball->yVel = -ball->yVel;
        }
        
        // Sweep across each paddle's inward face. Using the current paddle
        // position avoids stale collision tags and catches fast crossings.
        const float halfWidth = std::abs(t->GetLocalScale().x * box->relativeWidth) * 0.5f;
        const float halfHeight = std::abs(t->GetLocalScale().y * box->relativeHeight) * 0.5f;
        for (EntityID paddle : SceneView<PongMovement2D, CollisionBox2D, Transform3D>(*scene)) {
            const glm::vec4 bounds = getBounds(scene->Get<CollisionBox2D>(paddle), scene->Get<Transform3D>(paddle));
            const bool movingLeft = ball->xVel < 0.0f;
            const float face = movingLeft ? bounds.z + halfWidth : bounds.x - halfWidth;
            const bool crossed = movingLeft ? previous.x >= face && pos.x <= face
                                            : previous.x <= face && pos.x >= face;
            if (!crossed || pos.x == previous.x) continue;
            const float fraction = (face - previous.x) / (pos.x - previous.x);
            const float hitY = previous.y + (pos.y - previous.y) * fraction;
            if (hitY + halfHeight < bounds.y || hitY - halfHeight > bounds.w) continue;
            pos.x = face + (face - pos.x);
            ball->xVel = -ball->xVel;
            break;
        }

        t->SetLocalPosition(pos);
    }
}

SYSTEM_ON_UPDATE(FightBall2DSystem) 
{
    for (EntityID ent: SceneView<FightBall2D, CollisionBox2D, Transform3D>(*scene))
    {

        FightBall2D *ball = scene->Get<FightBall2D>(ent);
        CollisionBox2D *box = scene->Get<CollisionBox2D>(ent);
        Transform3D *t = scene->Get<Transform3D>(ent);

        if (OnPress(input, "R")) {
            ball->bouncing = true;
            ball->vel = ball->vertVel;
            t->SetLocalPosition(ball->orgPos);
            continue;
        }
        glm::vec4 bounds = getBounds(box, t);
        glm::vec3 pos = t->GetLocalPosition();
        if (ball->bouncing) {
            ball->vel += ball->gravity * deltaTime;
            pos.y += ball->vel * deltaTime;
            
            if (pos.y > ball->floor) {
                pos.y = ball->floor;
                ball->vel = -std::abs(ball->vertVel);
            }

            t->SetLocalPosition(pos);
            bounds = getBounds(box, t);
            for (EntityID fighterEnt: SceneView<Fighter2D, CollisionBox2D, Transform3D>(*scene))
            {
                Fighter2D *fighter = scene->Get<Fighter2D>(fighterEnt);
                CollisionBox2D *entBox = scene->Get<CollisionBox2D>(fighterEnt);
                Transform3D *entT = scene->Get<Transform3D>(fighterEnt);

                glm::vec4 entBounds = getBounds(entBox, entT);
                
                if (collide(entBounds, bounds) && fighter->attacking) {
                    ball->leftFacing = fighter->leftFacing;
                    ball->vel = ball->horVel;
                    ball->bouncing = false;
                    // Place the launched ball outside its hitter.
                    f32 halfWidth = std::abs(t->GetLocalScale().x * box->relativeWidth) / 2;
                    pos.x = ball->leftFacing ? entBounds.x - halfWidth - 0.001f
                                             : entBounds.z + halfWidth + 0.001f;
                    break;
                }
            }
        }
        else 
        {
            if (ball->leftFacing) {
                pos.x -= ball->vel * deltaTime;
            }
            else {
                pos.x += ball->vel * deltaTime;
            }

            t->SetLocalPosition(pos);
            bounds = getBounds(box, t);
            for (EntityID fighterEnt: SceneView<Fighter2D, CollisionBox2D, Transform3D>(*scene))
            {
                Fighter2D *fighter = scene->Get<Fighter2D>(fighterEnt);
                CollisionBox2D *entBox = scene->Get<CollisionBox2D>(fighterEnt);
                Transform3D *entT = scene->Get<Transform3D>(fighterEnt);

                glm::vec4 entBounds = getBounds(entBox, entT);
                
                if (collide(entBounds, bounds)) {
                    if (fighter->attacking) 
                    {
                        ball->vel = -std::abs(ball->vertVel);
                        ball->bouncing = true;
                        // Separate the popped ball so this attack cannot hit it again.
                        pos.y = entBounds.y - std::abs(t->GetLocalScale().y * box->relativeHeight) / 2 - 0.001f;
                        break;
                    }
                    else 
                    {
                        entT->SetLocalPosition({-1000, -1000, -1000});;
                        break;
                    }
                }
                
            }
        }

        t->SetLocalPosition(pos);
    }
}
SYSTEM_ON_UPDATE(Fighter2DSystem)
{
    for (EntityID ent: SceneView<Fighter2D, CollisionBox2D, MeshComponent, Transform3D>(*scene))
    {
        Fighter2D *fighter = scene->Get<Fighter2D>(ent);
        MeshComponent *entMesh = scene->Get<MeshComponent>(ent);
        CollisionBox2D *entBox = scene->Get<CollisionBox2D>(ent);
        Transform3D *entT = scene->Get<Transform3D>(ent);

        if (OnPress(input, "R")) {
            entT->SetLocalPosition(fighter->orgPos);
            continue;
        }

        if (OnPress(input, fighter->attackButton)) {
            if (!fighter->attacking && fighter->attackCooldownLeft <= 0) {
                fighter->attacking = true;
                fighter->attackCooldownLeft = fighter->attackCooldown;
                fighter->attackDurationLeft = fighter->attackDuration;
            }
        }

        if (OnPress(input, fighter->dashButton)) {
            if (!fighter->dashing && fighter->dashCooldownLeft <= 0) {
                fighter->dashing = true;
                fighter->dashCooldownLeft = fighter->dashCooldown;
                fighter->dashDurationLeft = fighter->dashDuration;
            }
        }

        glm::vec3 pos = entT->GetLocalPosition();
        glm::vec4 bounds = getBounds(entBox, entT);
        if (fighter->dashing) 
        {
            fighter->dashDurationLeft -= deltaTime;
            if (fighter->leftFacing) 
            {
                pos.x -= fighter->dashSpeed * deltaTime;
            }
            else 
            {
                pos.x += fighter->dashSpeed * deltaTime;
            }
        }
        else
        {
            if (OnHold(input, fighter->leftButton)) {
                pos.x -= fighter->moveSpeed * deltaTime;
                pos.x = std::max(pos.x, fighter->leftBound);
            }
            if (OnHold(input, fighter->rightButton)) {
                pos.x += fighter->moveSpeed * deltaTime;
                pos.x = std::min(pos.x, fighter->rightBound);
            }
        }


        entT->SetLocalPosition(pos);
        bounds = getBounds(entBox, entT);
        fighter->attackCooldownLeft -= deltaTime;
        if (fighter->attacking) {
            fighter->attackDurationLeft -= deltaTime;
            for (EntityID otherEnt: SceneView<Fighter2D, CollisionBox2D, Transform3D>(*scene))
            {
                if (otherEnt == ent) 
                {
                    continue;
                }
                Fighter2D *otherFighter = scene->Get<Fighter2D>(otherEnt);
                CollisionBox2D *otherBox = scene->Get<CollisionBox2D>(otherEnt);
                Transform3D *otherT = scene->Get<Transform3D>(otherEnt);

                glm::vec4 otherBounds = getBounds(otherBox, otherT);
                if (collide(otherBounds, bounds) && fighter->attacking) {
                    otherT->SetLocalPosition({-1000, -1000, -1000});
                    break;
                }
            }

            
            if (fighter->attackDurationLeft <= 0) {
                fighter->attacking = false;
                fighter->attackCooldownLeft = fighter->attackCooldown;
            }
        }
        entMesh->color = fighter->attacking ? fighter->attackColor : fighter->baseColor;

        fighter->dashCooldownLeft -= deltaTime;
        if (fighter->dashing && fighter->dashDurationLeft <= 0) {
            fighter->dashing = false;
            fighter->dashCooldownLeft = fighter->dashCooldown;
        }

        entT->SetLocalPosition(pos);
    }
}