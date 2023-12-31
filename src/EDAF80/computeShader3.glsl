#version 430 core

struct particleParameters{
    vec2 positions;
    vec2 velocities;
    vec2 predictedPosition;
	vec2 density;
	uvec3 spatialIndices;
	int spatial;
};

layout(binding = 0, std430) buffer dataBuffer {
    particleParameters particles[];
};
uniform float collisionDamping;
uniform float gravity;
uniform float particleRadius;
uniform float deltaTime;
uniform vec2 boundsSize;

uniform uint numParticles;
uniform float smoothingRadius;
uniform float targetDensity;
uniform float pressureMultiplier;
uniform float nearPressureMultiplier;
uniform float viscosityStrength;

//uniform vec2 interactionInputPoint;
//uniform float interactionInputStrength;
//uniform float interactionInputRadius;
const int hashK1 = 15823;
const int hashK2 = 9737333;
const float pi = 3.14159265359;

const ivec2 offsets2D[9] = ivec2[](
    ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1),
    ivec2(-1, 0), ivec2(0, 0), ivec2(1, 0),
    ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1)
);

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

ivec2 GetCell2D(float position,float radius){
    return (ivec2)(floor(position / radius));
}

uint HashCell2D(ivec2 cell){
       uint a = uint(cell.x) * hashK1;
       uint b = uint(cell.y) * hashK2;
       return (a + b);
}

uint KeyFromHash(uint hash,uint tableSize){
    return hash % tableSize;

float DensityKernel(float dst, float radius)
{
        if (dst < radius)
        {
            float v = radius - dst;
            float SpikyPow2ScalingFactor = 6 / (pow(smoothingRadius,4) * pi);
            return v * v  * SpikyPow2ScalingFactor;
        }
        return 0;
}
float NearDensityKernel(float dst, float radius)
{
    if (dst < radius)
    {
        float v = radius - dst;
        float SpikyPow3ScalingFactor = 10 / (pow(smoothingRadius,5) * pi);
        return v * v  * SpikyPow3ScalingFactor;
    }
    return 0;
}
float DensityDerivative(float dst, float radius)
{
    if (dst <= radius)
    {
        float v = radius - dst;
        float SpikyPow2DerivativeScalingFactor = 12 / (pow(smoothingRadius,4) * pi);
        return -v * SpikyPow2DerivativeScalingFactor;
    }
    return 0;
}
float NearDensityDerivative(float dst, float radius)
{
    if (dst <= radius)
    {
        float v = radius - dst;
        float SpikyPow3DerivativeScalingFactor = 30 / (pow(smoothingRadius,5) * pi);
        return -v * v * SpikyPow3DerivativeScalingFactor;
    }
    return 0;
}
float ViscosityKernel(float dst, float radius)
{
    if (dst < radius)
    {
        float v = radius * radius - dst * dst;
        //pi
        float Poly6ScalingFactor = 4 / (pow(smoothingRadius,8) * pi);
        return v * v * v * Poly6ScalingFactor;
    }
    return 0;
}
// GLSL中的vec2, ivec2, uvec3类型对应HLSL中的float2, int2, uint3
vec2 CalculateDensity(vec2 pos) {
    //getcell2d 需要被实现或者找个替代
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;
    float density = 0.0;
    float nearDensity = 0.0;

    // 邻居搜索
    for (int i = 0; i < 9; ++i) {
        //hashcell2d同理，也需要找个替代或者实现这个函数
        uint hash = HashCell2D(originCell + offsets2D[i]);
        //keyfromhash也是内置函数。需要替代或者被实现
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = particleParameters[key].SpatialOffsets;

        while (currIndex < numParticles) {
            uvec3 indexData = particleParameters[currIndex].SpatialIndices;
            currIndex++;
            // 退出，如果不再查看正确的数据
            if (indexData.z != key) break;
            // 跳过，如果哈希不匹配
            if (indexData.y != hash) continue;

            uint neighbourIndex = indexData.x;
            vec2 neighbourPos = particleParameters[neighbourIndex].predictedPosition;
            vec2 offsetToNeighbour = neighbourPos - pos;
            float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

            // 跳过，如果不在半径内
            if (sqrDstToNeighbour > sqrRadius) continue;

            // 计算密度和近似密度
            float dst = sqrt(sqrDstToNeighbour);
            density += DensityKernel(dst, smoothingRadius);
            nearDensity += NearDensityKernel(dst, smoothingRadius);
        }
    }

    return vec2(density, nearDensity);
}
//根据密度计算压力
float PressureFromDensity(float density)
{
    return (density - targetDensity) * pressureMultiplier;
}

//计算近压力防止粒子过于聚集
float NearPressureFromDensity(float nearDensity)
{
    return nearPressureMultiplier * nearDensity;
}

vec2 ExternalForces(vec2 pos, vec2 velocity)
{
    // 重力
    vec2 gravityAccel = vec2(0.0, gravity);
    
    // 交互影响重力
    if (interactionInputStrength != 0.0) {
        vec2 inputPointOffset = interactionInputPoint - pos;  //交互点与粒子位置的偏移
        float sqrDst = dot(inputPointOffset, inputPointOffset);
        if (sqrDst < interactionInputRadius * interactionInputRadius)   //判断是否在交互影响半径
        {
            float dst = sqrt(sqrDst);   //实际距离
            float edgeT = (dst / interactionInputRadius);  //边缘距离的差值参数
            float centreT = 1.0 - edgeT;  //中心距离
            vec2 dirToCentre = inputPointOffset / dst;  //单位向量。指向交互中心
            float gravityWeight = 1.0 - (centreT * clamp(interactionInputStrength / 10.0, 0.0, 1.0));
            vec2 accel = gravityAccel * gravityWeight + dirToCentre * centreT * interactionInputStrength;
            accel -= velocity * centreT;
            return accel;
        }
    }
    return gravityAccel;
}
//更新位置，提前声明里面的sign函数我不确定是否可行，我自己写的不是这种，不过gpt说glsl包含这个函数，我就没动
// 碰撞处理函数
void HandleCollisions(uint particleIndex)
{
    vec2 pos = particleParameters[particleIndex].positions;
    vec2 vel = particleParameters[particleIndex].velocities;

    // 保持粒子在边界内
    const vec2 halfSize = boundsSize * 0.5;
    vec2 edgeDst = halfSize - abs(pos);

    if (edgeDst.x <= 0.0)
    {
        pos.x = halfSize.x * sign(pos.x);
        vel.x *= -1.0 * collisionDamping;
    }
    if (edgeDst.y <= 0.0)
    {
        pos.y = halfSize.y * sign(pos.y);
        vel.y *= -1.0 * collisionDamping;
    }
    // 更新位置和速度
    particleParameters[particleIndex].positions = pos;
    particleParameters[particleIndex].velocities = vel;
}

// Kernel函数（以ExternalForces函数为例）其实我看理论上是一个函数对应一个main函数，但是我看基本都一样，就弄到一个去了，不知道可行不可行
void main() {
    // 获取线程（Invocation）的全局索引
    uint particleIndex = gl_GlobalInvocationID.x;
    if (particleIndex >= numParticles) return;
    // ... 这里是ExternalForces的GLSL实现。函数1
    particleParameters[particleIndex].velocities += ExternalForces(particleParameters[particleIndex].positions, particleParameters[particleIndex].velocities) * deltaTime;
    // Predict
    const float predictionFactor = 1 / 120.0;
    particleParameters[particleIndex].predictedPosition = particleParameters[particleIndex].positions + particleParameters[particleIndex].velocities * predictionFactor;
  
    //处理碰撞，这也需要转换   函数2  我感觉跟后面的重复了
    //HandleCollisions(particleIndex); // 根据HLSL中的实现转换为GLSL
   
    //UpdateSpatialHash。函数3
    // 重置偏移量
    particleParameters[particleIndex].SpatialOffsets = numParticles;
    // 更新索引缓冲区
    uint index = particleIndex;
    ivec2 cell = GetCell2D(particleParameters[index].predictedPosition, smoothingRadius); 
    uint hash = HashCell2D(cell);
    uint key = KeyFromHash(hash, numParticles);
    particleParameters[particleIndex].SpatialIndices = uvec3(index, hash, key); // 使用 uvec3，因为 GLSL 中没有 uint3
    // 其他Kernel函数也需要以类似的方式转换
    
    //CalculateDensities 函数4
    vec2 pos = particleParameters[particleIndex].predictedPosition; // 取 vec4 的前两个分量作为位置
    particleParameters[particleIndex].Densities = CalculateDensity(pos);
    
    //CalculatePressureForce函数5
    float density = particleParameters[particleIndex].Densities.x; // 使用 .x 替代 [0]
    float densityNear = particleParameters[particleIndex].Densities.y; // 使用 .y 替代 [1]
    float pressure = PressureFromDensity(density);
    float nearPressure = NearPressureFromDensity(densityNear);
    vec2 pressureForce = vec2(0.0);

    vec2 pos = particleParameters[particleIndex].predictedPosition;
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;

    // 邻域搜索
    for (int i = 0; i < 9; i++) {
        uint hash = HashCell2D(originCell + offsets2D[i]);
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = particleParameters[key].SpatialOffsets;

        while (currIndex < numParticles) {
            uvec3 indexData = particleParameters[currIndex].SpatialIndices;
            currIndex++;
            // 如果不再查看正确的箱子则退出
            if (indexData.z != key) break; // 使用 .z 替代 [2]
            // 如果哈希不匹配则跳过
            if (indexData.y != hash) continue; // 使用 .y 替代 [1]

            uint neighbourIndex = indexData.x; // 使用 .x 替代 [0]
            // 如果是自己则跳过
            if (neighbourIndex ==[particleIndex] continue;

            vec2 neighbourPos = particleParameters[neighbourIndex].predictedPosition;
            vec2 offsetToNeighbour = neighbourPos - pos;
            float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

            // 如果不在半径内则跳过
            if (sqrDstToNeighbour > sqrRadius) continue;

            // 计算压力
            float dst = sqrt(sqrDstToNeighbour);
            vec2 dirToNeighbour = dst > 0.0 ? offsetToNeighbour / dst : vec2(0.0, 1.0);

            float neighbourDensity = particleParameters[neighbourIndex].Densities.x;
            float neighbourNearDensity = particleParameters[neighbourIndex].Densities.y;
            float neighbourPressure = PressureFromDensity(neighbourDensity);
            float neighbourNearPressure = NearPressureFromDensity(neighbourNearDensity);

            float sharedPressure = (pressure + neighbourPressure) * 0.5;
            float sharedNearPressure = (nearPressure + neighbourNearPressure) * 0.5;

            pressureForce += dirToNeighbour * DensityDerivative(dst, smoothingRadius) * sharedPressure / neighbourDensity;
            pressureForce += dirToNeighbour * NearDensityDerivative(dst, smoothingRadius) * sharedNearPressure / neighbourNearDensity;
        }
    }

    vec2 acceleration = pressureForce / density;
    particleParameters[particleIndex].velocities += acceleration * deltaTime; // 更新速度，使用 .xy 替代 [0]
    
    // CalculateViscosity 函数6
    vec2 pos = particleParameters[particleIndex].predictedPosition;
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;

    vec2 viscosityForce = vec2(0.0);
    vec2 velocity = particleParameters[particleIndex].velocities; // 获取 vec4 的前两个分量（假设是 vec4 类型）

       for (int i = 0; i < 9; i++) {
           uint hash = HashCell2D(originCell + offsets2D[i]);
           uint key = KeyFromHash(hash, numParticles);
           uint currIndex = particleParameters[key].SpatialOffsets;

           while (currIndex < numParticles) {
               uvec3 indexData = particleParameters[currIndex].SpatialIndices;
               currIndex++;
               // 如果不再查看正确的箱子则退出
               if (indexData.z != key) break; // 使用 .z 替代 [2]
               // 如果哈希不匹配则跳过
               if (indexData.y != hash) continue; // 使用 .y 替代 [1]

               uint neighbourIndex = indexData.x; // 使用 .x 替代 [0]
               // 如果是自己则跳过
               if (neighbourIndex ==[particleIndex]) continue;

               vec2 neighbourPos = particleParameters[neighbourIndex].predictedPosition;
               vec2 offsetToNeighbour = neighbourPos - pos;
               float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

               // 如果不在半径内则跳过
               if (sqrDstToNeighbour > sqrRadius) continue;

               float dst = sqrt(sqrDstToNeighbour);
               vec2 neighbourVelocity = particleParameters[neighbourIndex].velocities;
               viscosityForce += (neighbourVelocity - velocity) * ViscosityKernel(dst, smoothingRadius);
           }
       }
       particleParameters[particleIndex].velocities += viscosityForce * viscosityStrength * deltaTime;
    
    //UpdatePositions 函数7
    // 更新位置
    particleParameters[particleIndex].positions += particleParameters[particleIndex].velocities * deltaTime;
    // 处理碰撞
    HandleCollisions(particleIndex);
}
