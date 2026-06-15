# 보안 CAN 차량 네트워크 기반 자율주행 시스템

ATmega328P와 ERIKA Enterprise RTOS로 만든 차량 내부 네트워크 보안 통신 프로젝트다. 센서 ECU가 모은 주행 데이터를 암호화·서명해서 CAN으로 보내면, 제어 ECU가 검증·복호화한 뒤 차량 모델로 엔진 RPM을 계산한다.

---

## 개요

자율주행 상황을 가정한 차량 내부 네트워크(IVN) 보안 데모다. Arduino UNO(ATmega328P) 두 대를 OSEK/VDX 호환 RTOS인 ERIKA Enterprise(ERIKA3) 위에서 돌리고, 그 사이에 OP-TEE(ARM TrustZone 기반 신뢰실행환경)를 올린 NVIDIA Jetson Orin Nano를 보안 게이트웨이로 두었다. 노드 사이 통신은 MCP2517FD 컨트롤러(ACAN2517FD 라이브러리)로 클래식 CAN(CAN 2.0) 프레임을 주고받는다.

ECU끼리 메시지를 평문으로 주고받지 않는 것이 목표다. 경량 대칭키 암호(simple-DES)로 도청을 막고, 게이트웨이가 CAN ID 화이트리스트 검증과 공개키 서명(micro-RSA) 부착을 맡아 위·변조와 인가되지 않은 송신원을 걸러낸다.

이 저장소에는 양 끝단 Arduino ECU(Node1·Node2) 코드만 들어 있다. 중간 게이트웨이인 Orin Nano의 OP-TEE Trusted Application 코드는 범위 밖이고, 관련 내용은 `docs/`의 OP-TEE 가이드라인을 참고하면 된다.

## 시스템 아키텍처

![시스템 아키텍처](assets/images/architecture.svg)

<sub>Orin Nano OP-TEE Trusted Application 코드는 저장소 범위 밖(별도 관리)</sub>

| 구분 | ① Node1 — 센서 ECU | ② Orin Nano — 게이트웨이 | ③ Node2 — 제어 ECU |
|------|----------------------|---------------------------|----------------------|
| 플랫폼 | Arduino UNO + ERIKA | Jetson Orin Nano + OP-TEE | Arduino UNO + ERIKA |
| 역할 | 센서 수집 → 암호화 → 송신 | 화이트리스트 검증 → 서명 부착 → ID 변환 | 검증 → 복호화 → RPM 제어 |
| 입력 | 스로틀(A1), 라이다(A2), 버튼(A0) | CAN `0x123` | CAN `0x456` |
| 보안 | `encrypt()` 40-bit simple-DES | `set_whitelist` · `set_priv_key(0x1234)` · 서명 생성 | `decrypt()` + `micro_verify()` |
| 주요 태스크 | `Task1`, `TASK_readADC`, `Task2`, `ButtonISR` | (OP-TEE TA, 저장소 외부) | `ReadCAN`, `SensorTask` |
| 부가 | 버튼으로 카메라 방향 선택 | `generate_public_key()` | `Car_Model` RPM 모델, 힙 측정 유틸 |

## 보안 메커니즘

| 구성요소 | 구현 위치 | 설명 |
|----------|----------|------|
| 대칭키 암호화 | Node1 `encrypt()` | 40-bit(5-byte) 블록 `simple_des`, 2-라운드 Feistel, 키 `0x1234` |
| 라운드 함수 | `feistel()` | XOR과 비트 회전 기반 경량 혼합 함수 |
| CAN ID 화이트리스트 | Orin Nano(OP-TEE) | `set_whitelist(0x123)` 으로 인가된 송신원만 통과 |
| 서명 생성 | Orin Nano(OP-TEE) | 신뢰실행환경 안에서 개인키로 3-byte 서명 생성 후 ID `0x456` 으로 전달 |
| 공개키 서명검증 | Node2 `micro_verify()` | 24-bit 미니 RSA(공개지수 `e=17`)로 서명 복원·대조 |
| 해시 | Node2 `mini_hash24()` | 메시지 무결성용 24-bit 커스텀 해시 |
| 대칭키 복호화 | Node2 `decrypt()` | 같은 키 `0x1234` 로 원본 센서값 복원 |

암호 구현은 8비트 MCU 학습·데모용 경량 알고리즘이라, 실제 차량에 쓸 만한 안전성을 보장하지는 않는다.

## CAN 프레임 포맷 (8 byte)

![CAN 프레임 바이트 구성](assets/images/can-frame.svg)

페이로드 5바이트(센서 데이터)는 두 구간 모두 암호화돼 있고, 게이트웨이를 지나며 뒤 3바이트에 서명이 채워진다.

| 바이트 | 필드 | ① Node1 → Orin (`0x123`) | ② Orin → Node2 (`0x456`) |
|--------|------|--------------------------|--------------------------|
| `[0]`   | camera | 카메라 방향 (암호화됨) | 〃 |
| `[1:2]` | throttle | 스로틀 ADC 16-bit (암호화됨) | 〃 |
| `[3:4]` | lidar | 라이다 ADC 16-bit (암호화됨) | 〃 |
| `[5:7]` | signature | `0x00 00 00` (서명자리 비움) | OP-TEE가 생성한 3-byte 서명 |

원본 평문은 `[0]`=camera(0:None/Down, 1:Front/Up, 2:Left, 3:Right), `[1:2]`=throttle, `[3:4]`=lidar 순서이고, 16-bit 값은 big-endian이다.

### 바이트 분할 (byte-split)

송신측은 16-bit ADC 값을 상·하위 두 바이트로 쪼개 싣고, 수신측은 복호화한 뒤 두 바이트를 다시 16-bit로 합친다.

```c
// 송신 — Node1/asw.c
buf_send[0] =  camera_adc        & 0xff;   // [0] 카메라 방향
buf_send[1] = (throttle_adc >> 8) & 0xff;  // [1] throttle 상위 바이트
buf_send[2] =  throttle_adc       & 0xff;  // [2] throttle 하위 바이트
buf_send[3] = (lidar_adc >> 8) & 0xff;     // [3] lidar 상위 바이트
buf_send[4] =  lidar_adc       & 0xff;     // [4] lidar 하위 바이트
buf_send[5] = buf_send[6] = buf_send[7] = 0; // [5:7] 서명자리(0으로 초기화)

// 수신 — Node2/asw.c (복호화 후 재조립)
camera   =  decrypted[0];
throttle = (decrypted[1] << 8) | decrypted[2];
lidar    = (decrypted[3] << 8) | decrypted[4];
```

송신(Node1)과 수신(Node2) 모두 `[1:2]=throttle`, `[3:4]=lidar` 순서로 맞춰 두었다.

### 처리 흐름

1. Node1 — 센서값을 프레임에 담고 앞 5바이트(`[0:5]`)를 DES로 암호화한다. 서명자리(`[5:7]`)는 0으로 두고 10ms 주기 알람으로 CAN `0x123` 에 실어 보낸다.
2. Orin Nano(OP-TEE) — `0x123` 이 화이트리스트에 있는지 확인하고, 개인키로 3바이트 서명을 만든 뒤 ID를 `0x456` 으로 바꿔 `[0:5]`=암호문, `[5:8]`=서명 형태로 다시 보낸다.
3. Node2 — `0x456` 을 받아 `micro_verify()` 로 서명을 검증하고 DES로 복호화한다. 카메라 방향에 따라 `SensorTask` 이벤트를 띄우고, 라이다 거리(`<500`)와 함께 회피/기본 모드를 골라 `control_rpm()` 을 호출한다.

## 동작 결과

데이터가 Node1 → OP-TEE 게이트웨이 → Node2 로 흐르며 암호화·서명·검증되는 과정을 시리얼·터미널 캡처로 남겼다.

### Node1 — 센서 수집과 암호화 송신
![Node1 송신 시리얼](assets/images/demo-1-node1-tx.png)

`raw` 가 원본 센서값(throttle·lidar·camera)이고, 이걸 DES로 암호화한 게 `enc` 다. 이 5바이트와 빈 서명자리(`0 0 0`)를 CAN `0x123` 으로 보낸다.

### Orin Nano(OP-TEE) — 화이트리스트 검증과 서명 생성
![OP-TEE 게이트웨이](assets/images/demo-2-optee-gateway.png)

TrustZone 보안 영역(OP-TEE TA)에서 `whitelist = 0x123` 과 `private_key` 를 저장하고, 메시지 서명 3바이트를 만들어 `signature generated successfully!` 를 출력한다. 그다음 CAN ID를 `0x456` 으로 바꿔 Node2로 넘긴다.

### Node2 — 서명 검증과 복호화
![Node2 검증 시리얼](assets/images/demo-3-node2-verify.png)

받은 `enc + sig` 에 `micro_verify` 를 돌린 결과가 `1`(성공)이고 `Hash` 와 `decrypted_sig` 가 같다. 검증을 통과하면 복호화해서 원본 `raw` 센서값을 되살린다.

### Node2 — 차량 모델 RPM 제어
![RPM 제어 시리얼](assets/images/demo-4-node2-rpm.png)

복원한 센서값으로 `control_rpm()` 을 호출한다. 카메라가 물체를 잡고(`Front`/`Left`/`Right`) 라이다 거리가 500 이하면 회피 모드로 RPM을 낮추고(`rpm:1000`), 아무것도 없으면(`None`) 정상 주행(`rpm:3659`)으로 돈다.

## 시연 영상

Node1 → OP-TEE → Node2 순서로 각 노드의 실제 동작 영상이다. 썸네일을 누르면 유튜브로 넘어간다.

| ① Node1 — 센서·암호화 | ② Orin Nano — OP-TEE | ③ Node2 — 검증·RPM 제어 |
|:---:|:---:|:---:|
| [![Node1 동작](https://img.youtube.com/vi/Tj3xIFcjZZo/hqdefault.jpg)](https://youtu.be/Tj3xIFcjZZo) | [![OP-TEE 동작](https://img.youtube.com/vi/tUidN5c58tw/hqdefault.jpg)](https://youtu.be/tUidN5c58tw) | [![Node2 동작](https://img.youtube.com/vi/ZAuceCu_vr0/hqdefault.jpg)](https://youtu.be/ZAuceCu_vr0) |
| 센서 수집 → DES 암호화 → CAN `0x123` 송신 | 화이트리스트 검증 → 서명 생성 → ID `0x456` 변환 | 서명검증·복호화 → 카메라/라이다 기반 RPM 제어 |

## 디렉토리 구조

```
.
├── Node1/                  # 센서/송신 ECU
│   ├── asw.c               # 애플리케이션 SW (태스크/ISR: ADC 수집·암호화·송신)
│   ├── bsw.cpp / bsw.h     # 기반 SW (CAN 드라이버 래퍼, DES 암호, 시리얼)
│   ├── SPI.cpp / SPI.h     # SPI 드라이버
│   ├── 1.html              # 자율주행 UI 프로토타입(웹)
│   ├── conf.oil            # ERIKA OS 구성 (태스크/알람/ISR 정의)
│   ├── Makefile            # 코드생성·빌드·업로드
│   └── lib/ACAN2517FD/     # MCP2517FD CAN-FD 컨트롤러 라이브러리
│
├── Node2/                  # 제어/수신 ECU
│   ├── asw.c               # 애플리케이션 SW (수신·검증·복호화·RPM 제어)
│   ├── bsw.cpp / bsw.h     # 기반 SW (DES 복호화, RSA 서명검증, 해시, 메모리 유틸)
│   ├── Car_Model.cpp / .h  # 차량 물리 모델 (기어비 기반 RPM 계산)
│   ├── SPI.cpp / SPI.h
│   ├── conf.oil
│   ├── makefile
│   └── lib/ACAN2517FD/
│
├── assets/images/          # README용 동작 결과 캡처
└── docs/                   # 발표 PDF·평가항목·OP-TEE 가이드라인
```

`erika/` 와 `out/` 은 `make config` · `make build` 때 자동으로 만들어지는 산출물이라 `.gitignore` 로 빼 두었다.

## 하드웨어 구성

| 항목 | 사양 |
|------|------|
| ECU (Node1·Node2) | ATmega328P (Arduino UNO) |
| 보안 게이트웨이 | NVIDIA Jetson Orin Nano + OP-TEE (ARM TrustZone) |
| RTOS (ECU) | ERIKA Enterprise (ERIKA3), OSEK ECC2 |
| CAN 컨트롤러 | MCP2517FD (CAN-FD 지원 칩), SPI 연결 — CS: D9, INT: D2 |
| CAN 통신 | 클래식 CAN 2.0 프레임(`CAN_DATA`, 8-byte), 500 kbps, 비트레이트 스위치 미사용 |
| 시리얼 | 115200 bps |

## 빌드 & 업로드

Windows + Cygwin + Eclipse(ERIKA generator) + Arduino AVR 툴체인 환경 기준이다.

사전 준비
- ERIKA Enterprise 설치 경로: `C:\eclipse` (`generate_code.bat` 포함)
- Arduino IDE 1.8.16 (AVR 코어): `C:\Arduino`
- 업로드 포트: Node1 = `COM3`, Node2 = `COM6` (환경에 맞게 `makefile` 수정)

각 노드 디렉토리에서 실행한다.

```bash
make config     # conf.oil → erika/ · out/ 코드 자동 생성
make build      # AVR 크로스 컴파일 (→ out/arduino.hex)
make upload     # avrdude 로 보드에 플래시
```

## 설계 참고

- CAN ID가 `0x123`(Node1 송신)에서 `0x456`(Node2 수신)으로 바뀌는 건 의도된 설계다. 중간의 Orin Nano(OP-TEE)가 화이트리스트를 확인한 뒤 ID를 바꾸고 서명을 붙인다. 그래서 Node1이 서명자리(`[5:7]`)를 비워 보내고 Node2가 서명을 검증하는 비대칭 구조가 정상이다.
- 서명 생성 코드가 저장소에 없는 이유도 같다. 서명은 OP-TEE TA 안에서 만들어지고, 그 코드는 따로 관리한다(`docs/` OP-TEE 가이드라인 참고).
- ATmega328P는 SRAM이 2KB뿐이라, Node2에서 `searchRemainMemory()` / `freeMemory()` 로 힙 여유를 확인한다.
- 암호·서명은 8비트 MCU 학습용 경량 구현이다.

## 트러블슈팅 & 배운 점

개발하면서 부딪힌 문제와 해결 과정을 정리했다.

<details>
<summary>1. 송·수신 노드의 CAN 바이트 매핑 불일치</summary>

- 문제: Node1은 `[1:2]=throttle / [3:4]=lidar` 로 싣는데, Node2는 같은 자리를 `lidar / throttle` 로 읽어서 두 센서값이 뒤바뀌었다.
- 원인: 두 노드가 공유해야 할 프레임 포맷 명세 없이 송·수신 코드를 따로 작성했다.
- 해결: 송신측을 기준으로 수신 복원 순서를 맞추고, 프레임 포맷을 README와 다이어그램으로 적어 두었다.
- 배운 점: 이론상 단순해 보이던 CAN 송수신도, 실제로는 양 끝단의 바이트 약속(프레임 명세)이 정확히 맞아야 동작한다는 걸 체감했다.

</details>

<details>
<summary>2. CAN 수신 버퍼의 dangling pointer</summary>

- 문제: `CAN_readMsg()` 가 함수 지역(스택) 변수 `CANFDMessage.data` 의 주소를 반환해서, 함수가 끝난 뒤 무효 포인터를 참조했다.
- 원인: 수신 데이터의 수명(lifetime)을 고려하지 않고 포인터를 넘겼다.
- 해결: `static` 버퍼에 `memcpy` 로 복사한 뒤 그 주소를 반환하도록 바꿨다.
- 배운 점: 임베디드 C에서는 포인터가 가리키는 메모리의 수명(스택인지 정적인지)을 직접 따져야 한다는 걸 디버깅하면서 익혔다.

</details>

<details>
<summary>3. 형식적으로만 동작하던 서명 검증</summary>

- 문제: `micro_verify()` 결과를 출력만 하고, 검증에 실패해도 그대로 복호화·제어를 진행해서 검증이 사실상 무의미했다.
- 원인: 검증 반환값을 제어 흐름에 반영하지 않았다.
- 해결: 검증에 실패하면 메시지를 폐기(`TerminateTask`)하도록 분기를 넣었다.
- 배운 점: 검증을 '수행'하는 것과 검증 결과로 '동작을 막는' 것은 다른 문제이고, 보안은 후자까지 가야 의미가 있다는 걸 알았다.

</details>

<details>
<summary>4. OSEK 구성과 코드의 정합성 (리소스·태스크 종료)</summary>

- 문제: `ResRpm` 을 `conf.oil` 에 선언하지 않은 채 `#define ResRpm 0` 으로 썼고(ERIKA `RES_SCHEDULER` 와 충돌 소지), `ReadCAN` 태스크에 `TerminateTask()` 가 빠져 있었다(OSEK 위반).
- 원인: OIL 구성 파일과 애플리케이션 코드 사이의 정합성이 부족했다.
- 해결: `RESOURCE ResRpm` 을 정식으로 선언해 태스크에 할당하고, 빠진 `TerminateTask()` 를 채웠다.
- 배운 점: RTOS는 코드뿐 아니라 구성 파일(OIL)과의 약속(리소스 선언, 태스크 종료 규칙)까지 맞아야 제대로 돈다는 걸 체득했다.

</details>

### 기술적으로 어려웠던 점
- 2KB SRAM 안에서 RTOS와 CAN 드라이버, 암호화를 같이 돌리는 것 (힙 여유를 `freeMemory()` 로 확인)
- 8바이트 CAN 프레임에 암호문 5바이트와 서명 3바이트를 함께 넣는 페이로드 설계
- DES·RSA·해시를 8비트 MCU용 경량 알고리즘으로 구현
- Arduino ECU와 Jetson(OP-TEE/TrustZone) 사이의 이기종 게이트웨이 연동
- 타이머·버튼 ISR과 이벤트 기반 OSEK 멀티태스킹 설계

### 종합 회고

강의에서 이론으로만 다루던 CAN 통신을 실제 하드웨어(MCP2517FD)로 직접 구현해보고, ERIKA RTOS 환경에서 태스크·알람·인터럽트 타이밍을 직접 맞춰보면서 책으로는 얻기 힘든 실전 경험을 했다. 그 위에 암호화·서명 같은 보안 계층을 8비트 MCU에 얹고, Node1·Node2로 역할을 나눠 인터페이스를 맞추는 과정에서 이론과 실제의 간극을 체감했다.

## 팀 & 프로젝트 정보

국민대학교 2025학년도 1학기 「자율주행 컴퓨팅 플랫폼」 Final Project

| 이름 | 담당 |
|------|------|
| 조정빈 | Node1 — 센서 ECU |
| 김경재 | Node2 — 제어 ECU |

---
국민대학교 2025-1학기 「자율주행 컴퓨팅 플랫폼」 Final Project · 학습/연구용 임베디드 차량 보안 프로토타입
