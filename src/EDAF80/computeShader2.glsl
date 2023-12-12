#version 430 core

struct particleParameters{
    vec2 positions;
    vec2 velocities;
};

layout(binding = 0, std430) buffer dataBuffer {
    particleParameters particles[];
};
uniform float collisionDamping;
uniform float gravity;
uniform float particleRadius;
uniform float deltaTime;
uniform vec2 boundsSize;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint index = gl_GlobalInvocationID.x;
    //positions[index] += vec2(0.0, 0.01);
    //particles[index].positions += particles[index].velocities;
    vec2 pos = particles[index].positions;
    vec2 vel = particles[index].velocities;

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
    
    particles[index].positions = pos;
    particles[index].velocities = vel;
    particles[index].positions += particles[index].velocities * deltaTime;
    particles[index].velocities += vec2(0, gravity) * deltaTime;
}

void main() {
    // ��ȡ�̣߳�Invocation����ȫ������
    uint particleIndex = gl_GlobalInvocationID.x;
    if (particleIndex >= numParticles) return;
    // ... ������ExternalForces��GLSLʵ�֡�����1
    particles[particleIndex].velocities += ExternalForces(particles[particleIndex].positions, particles[particleIndex].velocities) * deltaTime;
    // Predict
    //const float predictionFactor = 1.0 / 120.0;
    float predictionFactor = 1.0 / 120.0;
    particles[particleIndex].predictedPosition = particles[particleIndex].positions + particles[particleIndex].velocities * predictionFactor;
  
    //������ײ����Ҳ��Ҫת��   ����2  �Ҹо���������ظ���
    //HandleCollisions(particleIndex); // ����HLSL�е�ʵ��ת��ΪGLSL
   
    //UpdateSpatialHash������3
    // ����ƫ����
    particles[particleIndex].SpatialOffsets = int(numParticles);
    // ��������������
    uint index = particleIndex;
    ivec2 cell = GetCell2D(particles[index].predictedPosition, smoothingRadius); 
    uint hash = HashCell2D(cell);
    uint key = KeyFromHash(hash, numParticles);
    particles[particleIndex].SpatialIndices = uvec3(index, hash, key); // ʹ�� uvec3����Ϊ GLSL ��û�� uint3
    // ����Kernel����Ҳ��Ҫ�����Ƶķ�ʽת��
    
    //CalculateDensities ����4
    vec2 pos = particles[particleIndex].predictedPosition; // ȡ vec4 ��ǰ����������Ϊλ��
    particles[particleIndex].Densities = CalculateDensity(pos);
    
    //CalculatePressureForce����5
    float density = particles[particleIndex].Densities.x; // ʹ�� .x ��� [0]
    float densityNear = particles[particleIndex].Densities.y; // ʹ�� .y ��� [1]
    float pressure = PressureFromDensity(density);
    float nearPressure = NearPressureFromDensity(densityNear);
    vec2 pressureForce = vec2(0.0);

    //vec2 pos = particles[particleIndex].predictedPosition;
    ivec2 originCell = GetCell2D(pos, smoothingRadius);
    float sqrRadius = smoothingRadius * smoothingRadius;

    // ��������
    for (int i = 0; i < 9; i++) {
        uint hash = HashCell2D(originCell + offsets2D[i]);
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = particles[key].SpatialOffsets;

        while (currIndex < numParticles) {
            uvec3 indexData = particles[currIndex].SpatialIndices;
            currIndex++;
            // ������ٲ鿴��ȷ���������˳�
            if (indexData.z != key) break; // ʹ�� .z ��� [2]
            // �����ϣ��ƥ��������
            if (indexData.y != hash) continue; // ʹ�� .y ��� [1]

            uint neighbourIndex = indexData.x; // ʹ�� .x ��� [0]
            // ������Լ�������
            if (neighbourIndex == particleIndex) continue;

            vec2 neighbourPos = particles[neighbourIndex].predictedPosition;
            vec2 offsetToNeighbour = neighbourPos - pos;
            float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

            // ������ڰ뾶��������
            if (sqrDstToNeighbour > sqrRadius) continue;

            // ����ѹ��
            float dst = sqrt(sqrDstToNeighbour);
            vec2 dirToNeighbour = dst > 0.0 ? offsetToNeighbour / dst : vec2(0.0, 1.0);

            float neighbourDensity = particles[neighbourIndex].Densities.x;
            float neighbourNearDensity = particles[neighbourIndex].Densities.y;
            float neighbourPressure = PressureFromDensity(neighbourDensity);
            float neighbourNearPressure = NearPressureFromDensity(neighbourNearDensity);

            float sharedPressure = (pressure + neighbourPressure) * 0.5;
            float sharedNearPressure = (nearPressure + neighbourNearPressure) * 0.5;

            pressureForce += dirToNeighbour * DensityDerivative(dst, smoothingRadius) * sharedPressure / neighbourDensity;
            pressureForce += dirToNeighbour * NearDensityDerivative(dst, smoothingRadius) * sharedNearPressure / neighbourNearDensity;
        }
    }

    vec2 acceleration = pressureForce / density;
    particles[particleIndex].velocities += acceleration * deltaTime; // �����ٶȣ�ʹ�� .xy ��� [0]
    
    // CalculateViscosity ����6
    //vec2 pos = particles[particleIndex].predictedPosition;
    //ivec2 originCell = GetCell2D(pos, smoothingRadius);
    //float sqrRadius = smoothingRadius * smoothingRadius;

    vec2 viscosityForce = vec2(0.0);
    vec2 velocity = particles[particleIndex].velocities; // ��ȡ vec4 ��ǰ���������������� vec4 ���ͣ�

       for (int i = 0; i < 9; i++) {
           uint hash = HashCell2D(originCell + offsets2D[i]);
           uint key = KeyFromHash(hash, numParticles);
           uint currIndex = particles[key].SpatialOffsets;

           while (currIndex < numParticles) {
               uvec3 indexData = particles[currIndex].SpatialIndices;
               currIndex++;
               // ������ٲ鿴��ȷ���������˳�
               if (indexData.z != key) break; // ʹ�� .z ��� [2]
               // �����ϣ��ƥ��������
               if (indexData.y != hash) continue; // ʹ�� .y ��� [1]

               uint neighbourIndex = indexData.x; // ʹ�� .x ��� [0]
               // ������Լ�������
               if (neighbourIndex == particleIndex) continue;

               vec2 neighbourPos = particles[neighbourIndex].predictedPosition;
               vec2 offsetToNeighbour = neighbourPos - pos;
               float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

               // ������ڰ뾶��������
               if (sqrDstToNeighbour > sqrRadius) continue;

               float dst = sqrt(sqrDstToNeighbour);
               vec2 neighbourVelocity = particles[neighbourIndex].velocities;
               viscosityForce += (neighbourVelocity - velocity) * ViscosityKernel(dst, smoothingRadius);
           }
       }
       particles[particleIndex].velocities += viscosityForce * viscosityStrength * deltaTime;
    
    //UpdatePositions ����7
    // ����λ��
    particles[particleIndex].positions += particles[particleIndex].velocities * deltaTime;
    // ������ײ
    HandleCollisions(particleIndex);
}
