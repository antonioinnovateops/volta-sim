import { useRef } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
import { OrbitControls, Text, RoundedBox } from "@react-three/drei";
import * as THREE from "three";
import type { GpioState, PwmChannel, AdcChannel } from "../types";

interface BoardViewerProps {
  gpio: GpioState;
  pwm: PwmChannel[];
  adc: AdcChannel[];
}

// LED positions on STM32F4 Discovery (compass layout)
const LEDS: { pin: number; color: string; label: string; pos: [number, number, number] }[] = [
  { pin: 12, color: "#00ff00", label: "LD4", pos: [0, 0.15, -0.8] },   // Green (top)
  { pin: 13, color: "#ff8800", label: "LD3", pos: [0.8, 0.15, 0] },    // Orange (right)
  { pin: 14, color: "#ff0000", label: "LD5", pos: [0, 0.15, 0.8] },    // Red (bottom)
  { pin: 15, color: "#0088ff", label: "LD6", pos: [-0.8, 0.15, 0] },   // Blue (left)
];

function LEDMesh({
  position,
  color,
  isOn,
  label,
}: {
  position: [number, number, number];
  color: string;
  isOn: boolean;
  label: string;
}) {
  const meshRef = useRef<THREE.Mesh>(null);
  const matRef = useRef<THREE.MeshStandardMaterial>(null);
  const targetIntensity = useRef(0);

  useFrame(() => {
    if (!matRef.current) return;
    targetIntensity.current = isOn ? 4 : 0;
    const current = matRef.current.emissiveIntensity;
    matRef.current.emissiveIntensity += (targetIntensity.current - current) * 0.15;
  });

  return (
    <group position={position}>
      <mesh ref={meshRef}>
        <sphereGeometry args={[0.12, 16, 16]} />
        <meshStandardMaterial
          ref={matRef}
          color={isOn ? color : "#222"}
          emissive={color}
          emissiveIntensity={0}
          transparent
          opacity={isOn ? 0.95 : 0.4}
        />
      </mesh>
      <Text
        position={[0, 0.22, 0]}
        fontSize={0.1}
        color="#888"
        anchorX="center"
        anchorY="bottom"
        rotation={[-Math.PI / 2, 0, 0]}
      >
        {label}
      </Text>
    </group>
  );
}

function MotorMesh({ dutyCycle, enabled }: { dutyCycle: number; enabled: boolean }) {
  const groupRef = useRef<THREE.Group>(null);
  const bladeRef = useRef<THREE.Mesh>(null);
  const speed = useRef(0);

  useFrame((_, delta) => {
    const targetSpeed = enabled ? dutyCycle * 30 : 0;
    speed.current += (targetSpeed - speed.current) * 0.05;
    if (bladeRef.current) {
      bladeRef.current.rotation.y += speed.current * delta;
    }
  });

  return (
    <group ref={groupRef} position={[2.2, 0, 0]}>
      {/* Motor body */}
      <mesh position={[0, 0.2, 0]}>
        <cylinderGeometry args={[0.3, 0.3, 0.4, 16]} />
        <meshStandardMaterial color="#444" metalness={0.8} roughness={0.3} />
      </mesh>
      {/* Shaft */}
      <mesh position={[0, 0.55, 0]}>
        <cylinderGeometry args={[0.04, 0.04, 0.3, 8]} />
        <meshStandardMaterial color="#888" metalness={0.9} roughness={0.2} />
      </mesh>
      {/* Blade / propeller */}
      <mesh ref={bladeRef} position={[0, 0.72, 0]}>
        <boxGeometry args={[0.8, 0.02, 0.08]} />
        <meshStandardMaterial
          color={enabled ? "#e94560" : "#333"}
          emissive={enabled ? "#e94560" : "#000"}
          emissiveIntensity={enabled ? dutyCycle * 2 : 0}
        />
      </mesh>
      {/* Label */}
      <Text
        position={[0, -0.2, 0.4]}
        fontSize={0.12}
        color="#888"
        anchorX="center"
      >
        MOTOR
      </Text>
      {/* RPM text */}
      <Text
        position={[0, 1.0, 0]}
        fontSize={0.1}
        color={enabled ? "#e94560" : "#444"}
        anchorX="center"
      >
        {enabled ? `${Math.round(dutyCycle * 3000)} RPM` : "OFF"}
      </Text>
    </group>
  );
}

function ADCBar({ voltage, maxVoltage = 3.3 }: { voltage: number; maxVoltage?: number }) {
  const fillRef = useRef<THREE.Mesh>(null);
  const targetHeight = useRef(0);

  useFrame(() => {
    if (!fillRef.current) return;
    targetHeight.current = Math.max(0.01, (voltage / maxVoltage) * 1.5);
    const current = fillRef.current.scale.y;
    fillRef.current.scale.y += (targetHeight.current - current) * 0.1;
    fillRef.current.position.y = fillRef.current.scale.y / 2;
  });

  return (
    <group position={[-2.2, 0, 0]}>
      {/* Background bar */}
      <mesh position={[0, 0.75, 0]}>
        <boxGeometry args={[0.3, 1.5, 0.15]} />
        <meshStandardMaterial color="#111" transparent opacity={0.5} />
      </mesh>
      {/* Fill bar */}
      <mesh ref={fillRef} position={[0, 0, 0]}>
        <boxGeometry args={[0.28, 1, 0.13]} />
        <meshStandardMaterial
          color="#3a7bd5"
          emissive="#00d2ff"
          emissiveIntensity={0.5}
        />
      </mesh>
      {/* Label */}
      <Text
        position={[0, -0.2, 0.2]}
        fontSize={0.12}
        color="#888"
        anchorX="center"
      >
        ADC
      </Text>
      {/* Voltage text */}
      <Text
        position={[0, 1.7, 0]}
        fontSize={0.1}
        color="#3a7bd5"
        anchorX="center"
      >
        {voltage.toFixed(2)}V
      </Text>
    </group>
  );
}

function BoardPCB() {
  return (
    <group>
      {/* PCB */}
      <RoundedBox args={[2.8, 0.12, 3.2]} radius={0.06} position={[0, 0, 0]}>
        <meshStandardMaterial color="#0a5c1a" metalness={0.1} roughness={0.8} />
      </RoundedBox>
      {/* STM32 chip */}
      <mesh position={[0, 0.08, 0]}>
        <boxGeometry args={[0.6, 0.06, 0.6]} />
        <meshStandardMaterial color="#1a1a1a" metalness={0.4} roughness={0.5} />
      </mesh>
      {/* Label */}
      <Text
        position={[0, 0.12, 0]}
        fontSize={0.08}
        color="#aaa"
        anchorX="center"
        rotation={[-Math.PI / 2, 0, 0]}
      >
        STM32F407
      </Text>
      {/* Board title */}
      <Text
        position={[0, 0.08, -1.3]}
        fontSize={0.12}
        color="#ccc"
        anchorX="center"
        rotation={[-Math.PI / 2, 0, 0]}
      >
        STM32F4 DISCOVERY
      </Text>
      {/* USB connector */}
      <mesh position={[0, 0.06, 1.5]}>
        <boxGeometry args={[0.4, 0.08, 0.2]} />
        <meshStandardMaterial color="#888" metalness={0.9} roughness={0.2} />
      </mesh>
    </group>
  );
}

function Scene({ gpio, pwm, adc }: BoardViewerProps) {
  const portD = gpio["D"] || { pins: [] };
  const pinMap = new Map(portD.pins.map((p) => [p.pin, p.output === "HIGH"]));
  const pwmCh0 = pwm.find((c) => c.channel === 0);
  const adcCh0 = adc.find((c) => c.channel === 0);

  return (
    <>
      <ambientLight intensity={0.4} />
      <pointLight position={[5, 8, 5]} intensity={1.2} castShadow />
      <pointLight position={[-3, 4, -3]} intensity={0.5} color="#4488ff" />

      <BoardPCB />

      {LEDS.map((led) => (
        <LEDMesh
          key={led.pin}
          position={led.pos}
          color={led.color}
          isOn={pinMap.get(led.pin) || false}
          label={led.label}
        />
      ))}

      <MotorMesh
        dutyCycle={pwmCh0?.duty_cycle ?? 0}
        enabled={pwmCh0?.enabled ?? false}
      />

      <ADCBar voltage={adcCh0?.voltage ?? 0} />

      <OrbitControls
        makeDefault
        minDistance={3}
        maxDistance={10}
        enablePan={false}
        target={[0, 0.3, 0]}
      />
    </>
  );
}

export function BoardViewer({ gpio, pwm, adc }: BoardViewerProps) {
  return (
    <div style={{ width: "100%", height: "100%", background: "#0a0a1a" }}>
      <Canvas
        camera={{ position: [3, 3.5, 3], fov: 45 }}
        style={{ width: "100%", height: "100%" }}
      >
        <Scene gpio={gpio} pwm={pwm} adc={adc} />
      </Canvas>
    </div>
  );
}
