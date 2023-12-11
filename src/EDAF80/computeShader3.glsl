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
// GLSL�е�vec2, ivec2, uvec3���Ͷ�ӦHLSL�е�float2, int2, uint3
vec2 CalculateDensity(vec2 pos) {
    //getcell2d ��Ҫ��ʵ�ֻ����Ҹ����
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;
    float density = 0.0;
    float nearDensity = 0.0;

    // �ھ�����
    for (int i = 0; i < 9; ++i) {
        //hashcell2dͬ��Ҳ��Ҫ�Ҹ��������ʵ���������
        uint hash = HashCell2D(originCell + offsets2D[i]);
        //keyfromhashҲ�����ú�������Ҫ������߱�ʵ��
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = particleParameters[key].SpatialOffsets;

        while (currIndex < numParticles) {
            uvec3 indexData = particleParameters[currIndex].SpatialIndices;
            currIndex++;
            // �˳���������ٲ鿴��ȷ������
            if (indexData.z != key) break;
            // �����������ϣ��ƥ��
            if (indexData.y != hash) continue;

            uint neighbourIndex = indexData.x;
            vec2 neighbourPos = particleParameters[neighbourIndex].predictedPosition;
            vec2 offsetToNeighbour = neighbourPos - pos;
            float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

            // ������������ڰ뾶��
            if (sqrDstToNeighbour > sqrRadius) continue;

            // �����ܶȺͽ����ܶ�
            float dst = sqrt(sqrDstToNeighbour);
            density += DensityKernel(dst, smoothingRadius);
            nearDensity += NearDensityKernel(dst, smoothingRadius);
        }
    }

    return vec2(density, nearDensity);
}
//�����ܶȼ���ѹ��
float PressureFromDensity(float density)
{
    return (density - targetDensity) * pressureMultiplier;
}

//�����ѹ����ֹ���ӹ��ھۼ�
float NearPressureFromDensity(float nearDensity)
{
    return nearPressureMultiplier * nearDensity;
}

vec2 ExternalForces(vec2 pos, vec2 velocity)
{
    // ����
    vec2 gravityAccel = vec2(0.0, gravity);
    
    // ����Ӱ������
    if (interactionInputStrength != 0.0) {
        vec2 inputPointOffset = interactionInputPoint - pos;  //������������λ�õ�ƫ��
        float sqrDst = dot(inputPointOffset, inputPointOffset);
        if (sqrDst < interactionInputRadius * interactionInputRadius)   //�ж��Ƿ��ڽ���Ӱ��뾶
        {
            float dst = sqrt(sqrDst);   //ʵ�ʾ���
            float edgeT = (dst / interactionInputRadius);  //��Ե����Ĳ�ֵ����
            float centreT = 1.0 - edgeT;  //���ľ���
            vec2 dirToCentre = inputPointOffset / dst;  //��λ������ָ�򽻻�����
            float gravityWeight = 1.0 - (centreT * clamp(interactionInputStrength / 10.0, 0.0, 1.0));
            vec2 accel = gravityAccel * gravityWeight + dirToCentre * centreT * interactionInputStrength;
            accel -= velocity * centreT;
            return accel;
        }
    }
    return gravityAccel;
}
//����λ�ã���ǰ���������sign�����Ҳ�ȷ���Ƿ���У����Լ�д�Ĳ������֣�����gpt˵glsl��������������Ҿ�û��
// ��ײ������
void HandleCollisions(uint particleIndex)
{
    vec2 pos = particleParameters[particleIndex].positions;
    vec2 vel = particleParameters[particleIndex].velocities;

    // ���������ڱ߽���
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
    // ����λ�ú��ٶ�
    particleParameters[particleIndex].positions = pos;
    particleParameters[particleIndex].velocities = vel;
}

// Kernel��������ExternalForces����Ϊ������ʵ�ҿ���������һ��������Ӧһ��main�����������ҿ�������һ������Ū��һ��ȥ�ˣ���֪�����в�����
void main() {
    // ��ȡ�̣߳�Invocation����ȫ������
    uint particleIndex = gl_GlobalInvocationID.x;
    if (particleIndex >= numParticles) return;
    // ... ������ExternalForces��GLSLʵ�֡�����1
    particleParameters[particleIndex].velocities += ExternalForces(particleParameters[particleIndex].positions, particleParameters[particleIndex].velocities) * deltaTime;
    // Predict
    const float predictionFactor = 1 / 120.0;
    particleParameters[particleIndex].predictedPosition = particleParameters[particleIndex].positions + particleParameters[particleIndex].velocities * predictionFactor;
  
    //������ײ����Ҳ��Ҫת��   ����2  �Ҹо���������ظ���
    //HandleCollisions(particleIndex); // ����HLSL�е�ʵ��ת��ΪGLSL
   
    //UpdateSpatialHash������3
    // ����ƫ����
    particleParameters[particleIndex].SpatialOffsets = numParticles;
    // ��������������
    uint index = particleIndex;
    ivec2 cell = GetCell2D(particleParameters[index].predictedPosition, smoothingRadius); 
    uint hash = HashCell2D(cell);
    uint key = KeyFromHash(hash, numParticles);
    particleParameters[particleIndex].SpatialIndices = uvec3(index, hash, key); // ʹ�� uvec3����Ϊ GLSL ��û�� uint3
    // ����Kernel����Ҳ��Ҫ�����Ƶķ�ʽת��
    
    //CalculateDensities ����4
    vec2 pos = particleParameters[particleIndex].predictedPosition; // ȡ vec4 ��ǰ����������Ϊλ��
    particleParameters[particleIndex].Densities = CalculateDensity(pos);
    
    //CalculatePressureForce����5
    float density = particleParameters[particleIndex].Densities.x; // ʹ�� .x ��� [0]
    float densityNear = particleParameters[particleIndex].Densities.y; // ʹ�� .y ��� [1]
    float pressure = PressureFromDensity(density);
    float nearPressure = NearPressureFromDensity(densityNear);
    vec2 pressureForce = vec2(0.0);

    vec2 pos = particleParameters[particleIndex].predictedPosition;
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;

    // ��������
    for (int i = 0; i < 9; i++) {
        uint hash = HashCell2D(originCell + offsets2D[i]);
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = particleParameters[key].SpatialOffsets;

        while (currIndex < numParticles) {
            uvec3 indexData = particleParameters[currIndex].SpatialIndices;
            currIndex++;
            // ������ٲ鿴��ȷ���������˳�
            if (indexData.z != key) break; // ʹ�� .z ��� [2]
            // �����ϣ��ƥ��������
            if (indexData.y != hash) continue; // ʹ�� .y ��� [1]

            uint neighbourIndex = indexData.x; // ʹ�� .x ��� [0]
            // ������Լ�������
            if (neighbourIndex ==[particleIndex] continue;

            vec2 neighbourPos = particleParameters[neighbourIndex].predictedPosition;
            vec2 offsetToNeighbour = neighbourPos - pos;
            float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

            // ������ڰ뾶��������
            if (sqrDstToNeighbour > sqrRadius) continue;

            // ����ѹ��
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
    particleParameters[particleIndex].velocities += acceleration * deltaTime; // �����ٶȣ�ʹ�� .xy ��� [0]
    
    // CalculateViscosity ����6
    vec2 pos = particleParameters[particleIndex].predictedPosition;
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;

    vec2 viscosityForce = vec2(0.0);
    vec2 velocity = particleParameters[particleIndex].velocities; // ��ȡ vec4 ��ǰ���������������� vec4 ���ͣ�

       for (int i = 0; i < 9; i++) {
           uint hash = HashCell2D(originCell + offsets2D[i]);
           uint key = KeyFromHash(hash, numParticles);
           uint currIndex = particleParameters[key].SpatialOffsets;

           while (currIndex < numParticles) {
               uvec3 indexData = particleParameters[currIndex].SpatialIndices;
               currIndex++;
               // ������ٲ鿴��ȷ���������˳�
               if (indexData.z != key) break; // ʹ�� .z ��� [2]
               // �����ϣ��ƥ��������
               if (indexData.y != hash) continue; // ʹ�� .y ��� [1]

               uint neighbourIndex = indexData.x; // ʹ�� .x ��� [0]
               // ������Լ�������
               if (neighbourIndex ==[particleIndex]) continue;

               vec2 neighbourPos = particleParameters[neighbourIndex].predictedPosition;
               vec2 offsetToNeighbour = neighbourPos - pos;
               float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

               // ������ڰ뾶��������
               if (sqrDstToNeighbour > sqrRadius) continue;

               float dst = sqrt(sqrDstToNeighbour);
               vec2 neighbourVelocity = particleParameters[neighbourIndex].velocities;
               viscosityForce += (neighbourVelocity - velocity) * ViscosityKernel(dst, smoothingRadius);
           }
       }
       particleParameters[particleIndex].velocities += viscosityForce * viscosityStrength * deltaTime;
    
    //UpdatePositions ����7
    // ����λ��
    particleParameters[particleIndex].positions += particleParameters[particleIndex].velocities * deltaTime;
    // ������ײ
    HandleCollisions(particleIndex);
}
