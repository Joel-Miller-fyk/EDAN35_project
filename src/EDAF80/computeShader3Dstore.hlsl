#version 430 core

struct particleParameters{
    vec3 positions;
    vec3 velocities;
    vec3 predictedPosition;
	vec2 densities;
	uvec3 SpatialIndices;
	uint SpatialOffsets;
};
//����ֲ��������С����hlsl��numthreads���Ӧ���Ҹо�Ӧ�ø�2dһ��
layout(local_size_x = 125, local_size_y = 1, local_size_z = 1) in;

//������ Ū��һ��struct�Ļ��Ͳ����������
//bufferûŪ���������������õĶ��Ǹ�2d������ͬ�ģ��������������ȥ��
layout(binding = 0, std430) buffer dataBuffer {
    particleParameters particles[];
};

//uniform Ҫ��hlsl�е�const�������Ӧ
//uniform uint numParticles;
const uint numParticles = 10000;

uniform float gravity;
uniform float deltaTime;
uniform float collisionDamping;
uniform float smoothingRadius;
uniform float targetDensity;
uniform float pressureMultiplier;
uniform float nearPressureMultiplier;
uniform float viscosityStrength;
//uniform float edgeForce;
//uniform float edgeForceDst;
uniform vec3 boundsSize;
uniform vec3 centre;

uniform mat4 localToWorld;
uniform mat4 worldToLocal;

uniform vec2 interactionInputPoint;
uniform float interactionInputStrength;
uniform float interactionInputRadius;

const float PI = 3.1415926;


const uint hashK1 = 15823;
const uint hashK2 = 9737333;
const uint hashK3 = 440817757;

//��������ת���ˣ�2d����ֱ�Ӻϲ��ˣ������û�кϲ�ֱ�ӳ���
//�ڼ����ܶ��б��õ���ƽ���������Ե�
float SmoothingKernelPoly6(float dst, float radius)
{
    if (dst < radius)
    {
        float scale = 315 / (64 * PI * pow(abs(radius), 9));
        float v = radius * radius - dst * dst;
        return v * v * v * scale;
    }
    return 0;
}
//ͬ��
float SpikyKernelPow3(float dst, float radius)
{
    if (dst < radius)
    {
        float scale = 15 / (PI * pow(radius, 6));
        float v = radius - dst;
        return v * v * v * scale;
    }
    return 0;
}
//ͬ
float SpikyKernelPow2(float dst, float radius)
{
    if (dst < radius)
    {
        float scale = 15 / (2 * PI * pow(radius, 5));
        float v = radius - dst;
        return v * v * scale;
    }
    return 0;
}

float DerivativeSpikyPow3(float dst, float radius)
{
	if (dst <= radius)
	{
		float scale = 45 / (pow(radius, 6) * PI);
		float v = radius - dst;
		return -v * v * scale;
	}
	return 0;
}

float DerivativeSpikyPow2(float dst, float radius)
{
	if (dst <= radius)
	{
		float scale = 15 / (pow(radius, 5) * PI);
		float v = radius - dst;
		return -v * scale;
	}
	return 0;
}
//�����ܶ��ں�
float DensityKernel(float dst, float radius)
{
    return SpikyKernelPow2(dst, radius);
}
float NearDensityKernel(float dst, float radius)
{
    return SpikyKernelPow3(dst, radius);
}

float DensityDerivative(float dst, float radius)
{
    return DerivativeSpikyPow2(dst, radius);
}

float NearDensityDerivative(float dst, float radius)
{
    return DerivativeSpikyPow3(dst, radius);
}

//��������Ҫ������ת��
//�ܶ�-��ѹ����ת��
float PressureFromDensity(float density)
{
    return (density - targetDensity) * pressureMultiplier;
}
//���� ������ĺ�������
float NearPressureFromDensity(float nearDensity)
{
    return nearDensity * nearPressureMultiplier;
}

//��ײ����
void ResolveCollisions(uint particleIndex)
{
    // �ռ�ת�� ����buffer���Ѿ��������ˣ����ƶ�Ӧ����
    vec4 posWorld = vec4(particles[particleIndex].positions,1.0);
    vec3 posLocal = (worldToLocal * posWorld).xyz;
    vec4 velocityWorld = vec4(particles[particleIndex].velocities,0.0);
    vec3 velocityLocal = (worldToLocal,velocityWorld).xyz;

    // Calculate distance from box on each axis (negative values are inside box)
    const vec3 halfSize = vec3(0.5f);
    const vec3 edgeDst = halfSize - abs(posLocal);

    // Resolve collisions
    if (edgeDst.x <= 0)
    {
        posLocal.x = halfSize.x * sign(posLocal.x);
        velocityLocal.x *= -1 * collisionDamping;
    }
    if (edgeDst.y <= 0)
    {
        posLocal.y = halfSize.y * sign(posLocal.y);
        velocityLocal.y *= -1 * collisionDamping;
    }
    if (edgeDst.z <= 0)
    {
        posLocal.z = halfSize.z * sign(posLocal.z);
        velocityLocal.z *= -1 * collisionDamping;
    }

    // Transform resolved position/velocity back to world space
    particles[particleIndex].positions = (localToWorld * vec4(posLocal,1.0)).xyz;
    particles[particleIndex].velocities =(localToWorld * vec4(velocityLocal,0.0)).xyz;
}

//��ģ��2d���Ǹ�����ȷ���ǲ��ǶԵģ����ǹ����̵߳���ȷʵ��ȷ��������
//����һ�������һ�����⣬ͬ����2d�д��ڣ�����ÿ�������ҿ����ظ�������ĳ����������������ͬ�����������¶��壩���Ҳ�֪��Ӱ��󲻴������Ҫ�ֿ��Ļ����ҿ���ȥ�ֿ������Ǹо����������û���鷳������������ԵĻ����
void main(){
    uint particleIndex = gl_GlobalInvocationID.x;
    if (particleIndex >= numParticles) return;

    //������externelforces ������1
    // External forces (gravity)
    particles[particleIndex].velocities += vec3(0, gravity, 0) * deltaTime;
    // Predict
    particles[particleIndex].predictedPosition = particles[particleIndex].positions + particles[particleIndex].velocities * 1 / 120.0;
    
    //updateSpatialHash ����2
    // Reset offsets
    //SpatialOffsets[particleIndex] = numParticles;
    // Update index buffer
    //uint index = particleIndex;
    //ivec3 cell = GetCell3D(PredictedPositions[index], smoothingRadius);
    //uint hash = HashCell3D(cell);
    //uint key = KeyFromHash(hash, numParticles);
    //SpatialIndices[particleIndex] = uvec3(index, hash, key);
    
  
    //calculatedensities ��Ҫhash �汾
    vec3 pos = particles[particleIndex].predictedPosition;
    float sqrRadius = smoothingRadius * smoothingRadius;
    float density_cal = 0;
    float nearDensity = 0;
    int currIndex = 0;

    while (currIndex < numParticles)
        {
           
            currIndex ++;
            vec3 neighbourPos = particles[currIndex].predictedPosition;
            vec3 offsetToNeighbour = neighbourPos - pos;
            float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

                // Skip if not within radius
            if (sqrDstToNeighbour > sqrRadius) continue;

                // Calculate density and near density
            float dst = sqrt(sqrDstToNeighbour);
            density_cal += DensityKernel(dst, smoothingRadius);
            nearDensity += NearDensityKernel(dst, smoothingRadius);
            }

    particles[particleIndex].densities = vec2(density_cal, nearDensity);

    //calculatePressureForce ��Ҫhash�汾
    float density = particles[particleIndex].densities.x;
    float densityNear = particles[particleIndex].densities.y;
    float pressure = PressureFromDensity(density);
    float nearPressure = NearPressureFromDensity(densityNear);
    vec3 pressureForce = vec3(0.0f);
    int i = 0;
   // vec3 pos = PredictedPositions[particleIndex];
   // float sqrRadius = smoothingRadius * smoothingRadius;  //�ظ�������

    while (i < numParticles)
            {
                i ++;
                vec3 neighbourPos = particles[i].predictedPosition;
                vec3 offsetToNeighbour = neighbourPos - pos;
                float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

                // Skip if not within radiusvelocities
                if (sqrDstToNeighbour > sqrRadius) continue;

                // Calculate pressure force
                float densityNeighbour = particles[i].densities.x;
                float nearDensityNeighbour = particles[i].densities.y;
                float neighbourPressure = PressureFromDensity(densityNeighbour);
                float neighbourPressureNear = NearPressureFromDensity(nearDensityNeighbour);

                float sharedPressure = (pressure + neighbourPressure) / 2;
                float sharedNearPressure = (nearPressure + neighbourPressureNear) / 2;

                float dst = sqrt(sqrDstToNeighbour);
                vec3 dir = dst > 0 ? offsetToNeighbour / dst : vec3(0, 1, 0);

                pressureForce += dir * DensityDerivative(dst, smoothingRadius) * sharedPressure / densityNeighbour;
                pressureForce += dir * NearDensityDerivative(dst, smoothingRadius) * sharedNearPressure / nearDensityNeighbour;
            }

        vec3 acceleration = pressureForce / density;
        particles[particleIndex].velocities += acceleration * deltaTime;
    
  
    //calculateViscosity ��Ҫhash�汾
    //vec3 pos = PredictedPositions[particleIndex];   //�ظ�����
    vec3 viscosityForce = vec3(0.0f);
    vec3 velocity = particles[particleIndex].velocities;
    int index = 0;
            while (index < numParticles)
            {
                index ++;
                vec3 neighbourPos = particles[index].predictedPosition;
                vec3 offsetToNeighbour = neighbourPos - pos;
                float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

                // Skip if not within radius
                if (sqrDstToNeighbour > sqrRadius) continue;

                // Calculate viscosity
                float dst = sqrt(sqrDstToNeighbour);
                vec3 neighbourVelocity = particles[index].velocities;
                viscosityForce += (neighbourVelocity - velocity) * SmoothingKernelPoly6(dst, smoothingRadius);
            }

            particles[particleIndex].velocities += viscosityForce * viscosityStrength * deltaTime;
     
    //updatePositions ����6
    particles[particleIndex].positions += particles[particleIndex].velocities * deltaTime;
    ResolveCollisions(particleIndex);
}
