# RobotArm

<img src="https://github.com/user-attachments/assets/054cceab-fb13-40d9-bc92-c05daa685d3c" width="500" height="500"/>

# 프로젝트 개요 
- 로봇팔의 순기구학 제어 원리를 시각적으로 검증하기 위해 OpenGL 기반 시뮬레이션을 구현

# 개발환경 
- 개발언어: C++
- 라이브러리: OpenGL,GLM,GLAD
- 개발 툴 : visual studio 2022

# 주요기능 
## 1. 로봇 관절 제어 
- 각 관절에 대해 glm::translate, glm::rotate를 계층적으로 적용하여 4×4 변환행렬(Mat4) 생성
- 상위 관절의 변환이 하위 관절에 누적되도록 계층적 좌표계(Hierarchical Coordinate System) 구성
- 최종적으로 Base → Shoulder → Elbow → Wrist → Palm 순으로 연결된 Forward Kinematics 체인 구조 구현
- Shader에 model matrix를 전달해 실시간 렌더링 및 조명 반영
- 키보드(1~5) 및 마우스 입력으로 각 관절 회전값 실시간 제어 (Manual control)
### 관절 구조도
<img width="302" height="313" alt="image" src="https://github.com/user-attachments/assets/803d0719-fa1d-4cb0-a651-64fa9cc2b1e1" />
Base (1)
├── Shoulder (2)
│   └── Elbow (3)
│       └── Wrist (4)
│           └── Palm (5)
│               ├── Finger1
│               │   └── Finger1 Tip
│               └── Finger2
│                   └── Finger2 Tip

## 2. 객체 간 거리 기반 상호작용 
- Palm(손바닥)과 Object(주전자) 사이의 유클리드 거리 계산
- 거리 임계값(Threshold)과 손가락 각도(angle)에 따라 물체를 잡거나 놓는 상태 판단
- Forward Kinematics 결과 좌표를 이용해 grasp 동작 구현
- 물체를 놓을 경우, 현재 로봇팔 위치를 기준으로 중력 방향(−y축)으로 물체 낙하 시뮬레이션
## 3. 추상화(Abstraction) 기반 로봇 부품 구조 설계 
- Primitive 추상 클래스를 중심으로, Cylinder, Sphere, Plane이 이를 상속(inheritance) 하여 다형성(polymorphism) 으로 각 기하 객체를 생성
→ 코드 확장 시 다른 형태의 로봇 부품을 쉽게 추가 가능
