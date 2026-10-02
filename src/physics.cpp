#include "physics.h"

#include <algorithm>
#include <cmath>

static PhysicsVec3 add(const PhysicsVec3& a,const PhysicsVec3& b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static PhysicsVec3 sub(const PhysicsVec3& a,const PhysicsVec3& b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static PhysicsVec3 mul(const PhysicsVec3& a,float s){return {a.x*s,a.y*s,a.z*s};}
static float dot(const PhysicsVec3& a,const PhysicsVec3& b){return a.x*b.x+a.y*b.y+a.z*b.z;}

void physics_body_set_mass(PhysicsBody& body,float mass)
{
    body.mass=std::max(mass,0.0f);
    body.inverse_mass=(body.mass>0.0f)?1.0f/body.mass:0.0f;
    body.is_static=body.mass<=0.0f;
}

void physics_apply_force(PhysicsBody& body,const PhysicsVec3& force)
{
    if(!body.is_static) body.force=add(body.force,force);
}

void physics_apply_impulse(PhysicsBody& body,const PhysicsVec3& impulse)
{
    if(!body.is_static)
        body.velocity=add(body.velocity,mul(impulse,body.inverse_mass));
}

void physics_world_add_body(PhysicsWorld& world,PhysicsBody& body)
{
    if(std::find(world.bodies.begin(),world.bodies.end(),&body)==world.bodies.end())
        world.bodies.push_back(&body);
}

void physics_world_remove_body(PhysicsWorld& world,PhysicsBody& body)
{
    world.bodies.erase(std::remove(world.bodies.begin(),world.bodies.end(),&body),world.bodies.end());
}

bool physics_aabb_overlap(const PhysicsAabb& a,const PhysicsVec3& ap,const PhysicsAabb& b,const PhysicsVec3& bp)
{
    const PhysicsVec3 ac=add(ap,a.center), bc=add(bp,b.center);
    return std::fabs(ac.x-bc.x)<=a.half_extents.x+b.half_extents.x &&
           std::fabs(ac.y-bc.y)<=a.half_extents.y+b.half_extents.y &&
           std::fabs(ac.z-bc.z)<=a.half_extents.z+b.half_extents.z;
}

bool physics_raycast_ground(const PhysicsWorld& world,const PhysicsRay& ray,PhysicsRayHit& hit)
{
    hit=PhysicsRayHit{};
    if(std::fabs(ray.direction.y)<0.000001f) return false;
    const float t=(world.ground_y-ray.origin.y)/ray.direction.y;
    if(t<0.0f || t>ray.max_distance) return false;
    hit.hit=true; hit.distance=t;
    hit.point=add(ray.origin,mul(ray.direction,t));
    hit.normal={0.0f,1.0f,0.0f};
    return true;
}

void physics_world_step(PhysicsWorld& world,float dt)
{
    if(dt<=0.0f) return;
    dt=std::min(dt,0.05f);

    for(PhysicsBody* body:world.bodies){
        if(!body || body->is_static) continue;
        body->grounded=false;

        if(body->use_gravity)
            physics_apply_force(*body,mul(world.gravity,body->mass));

        const PhysicsVec3 acceleration=mul(body->force,body->inverse_mass);
        body->velocity=add(body->velocity,mul(acceleration,dt));

        const float damping=std::max(0.0f,1.0f-body->linear_damping*dt);
        body->velocity=mul(body->velocity,damping);
        body->position=add(body->position,mul(body->velocity,dt));

        const float bottom=body->position.y+body->collider.center.y-body->collider.half_extents.y;
        if(bottom<world.ground_y){
            body->position.y+=world.ground_y-bottom;
            if(body->velocity.y<0.0f)
                body->velocity.y=-body->velocity.y*body->restitution;
            if(std::fabs(body->velocity.y)<0.15f) body->velocity.y=0.0f;
            body->grounded=true;
        }

        body->force={};
    }

    /* Basic dynamic AABB separation on the smallest penetration axis. */
    for(std::size_t i=0;i<world.bodies.size();++i){
        PhysicsBody* a=world.bodies[i];
        if(!a) continue;
        for(std::size_t j=i+1;j<world.bodies.size();++j){
            PhysicsBody* b=world.bodies[j];
            if(!b || (a->is_static&&b->is_static)) continue;
            if(!physics_aabb_overlap(a->collider,a->position,b->collider,b->position)) continue;

            const PhysicsVec3 ac=add(a->position,a->collider.center);
            const PhysicsVec3 bc=add(b->position,b->collider.center);
            const float px=a->collider.half_extents.x+b->collider.half_extents.x-std::fabs(ac.x-bc.x);
            const float py=a->collider.half_extents.y+b->collider.half_extents.y-std::fabs(ac.y-bc.y);
            const float pz=a->collider.half_extents.z+b->collider.half_extents.z-std::fabs(ac.z-bc.z);

            PhysicsVec3 n{};
            float p=px;
            n.x=(ac.x<bc.x)?-1.0f:1.0f;
            if(py<p){p=py;n={0.0f,(ac.y<bc.y)?-1.0f:1.0f,0.0f};}
            if(pz<p){p=pz;n={0.0f,0.0f,(ac.z<bc.z)?-1.0f:1.0f};}

            if(a->is_static) b->position=add(b->position,mul(n,-p));
            else if(b->is_static) a->position=add(a->position,mul(n,p));
            else {
                a->position=add(a->position,mul(n,p*0.5f));
                b->position=add(b->position,mul(n,-p*0.5f));
            }

            /*
                Sequential impulse contact response.
                n points from body B toward body A.
            */
            const float inv_a =
                a->is_static ? 0.0f : a->inverse_mass;

            const float inv_b =
                b->is_static ? 0.0f : b->inverse_mass;

            const float inv_sum =
                inv_a + inv_b;

            if(inv_sum<=0.0f)
                continue;

            PhysicsVec3 relative_velocity =
                sub(a->velocity,b->velocity);

            const float velocity_along_normal =
                dot(relative_velocity,n);

            if(velocity_along_normal<0.0f){
                const float restitution =
                    std::min(
                        a->restitution,
                        b->restitution
                    );

                const float impulse_scalar =
                    -(1.0f+restitution) *
                    velocity_along_normal /
                    inv_sum;

                const PhysicsVec3 impulse =
                    mul(n,impulse_scalar);

                if(!a->is_static)
                    a->velocity=add(
                        a->velocity,
                        mul(impulse,inv_a)
                    );

                if(!b->is_static)
                    b->velocity=add(
                        b->velocity,
                        mul(impulse,-inv_b)
                    );

                /*
                    Coulomb friction impulse along the contact tangent.
                */
                relative_velocity =
                    sub(a->velocity,b->velocity);

                PhysicsVec3 tangent =
                    sub(
                        relative_velocity,
                        mul(
                            n,
                            dot(relative_velocity,n)
                        )
                    );

                const float tangent_length =
                    std::sqrt(
                        dot(tangent,tangent)
                    );

                if(tangent_length>0.000001f){
                    tangent =
                        mul(
                            tangent,
                            1.0f/tangent_length
                        );

                    float friction_scalar =
                        -dot(
                            relative_velocity,
                            tangent
                        ) /
                        inv_sum;

                    const float mu =
                        std::sqrt(
                            std::max(0.0f,a->friction) *
                            std::max(0.0f,b->friction)
                        );

                    const float friction_limit =
                        impulse_scalar * mu;

                    friction_scalar =
                        std::max(
                            -friction_limit,
                            std::min(
                                friction_limit,
                                friction_scalar
                            )
                        );

                    const PhysicsVec3 friction_impulse =
                        mul(
                            tangent,
                            friction_scalar
                        );

                    if(!a->is_static)
                        a->velocity=add(
                            a->velocity,
                            mul(friction_impulse,inv_a)
                        );

                    if(!b->is_static)
                        b->velocity=add(
                            b->velocity,
                            mul(friction_impulse,-inv_b)
                        );
                }
            }
        }
    }
}
