// Script package entry: spawn a cube and orbit it.
// Edit constants below and run `RinRinJs.Reload` in the UE console to see
// the change without restarting the editor.

const RADIUS = 200;
const SPEED = 1;
const HEIGHT_AMPLITUDE = 80;
const ROTATION_SPEED = 90;
const BASE_HEIGHT = 120;

let actor = 0;
let time = 0;

export function start(context) {
    time = 0;
    ue.log("[demo] start", JSON.stringify(context));

    actor = ue.spawnActorByPath("/Engine/BasicShapes/Cube.Cube", {
        location: { x: RADIUS, y: 0, z: BASE_HEIGHT },
        rotation: { pitch: 0, yaw: 0, roll: 0 },
        scale: { x: 1, y: 1, z: 1 },
    });

    if (!actor) {
        ue.log("[demo] failed to spawn actor");
    } else {
        ue.log("[demo] spawned actor handle=", actor);
    }
}

export function tick(deltaSeconds) {
    if (!actor) return;
    time += deltaSeconds;

    const angle = time * SPEED;
    ue.setLocation(actor, {
        x: Math.cos(angle) * RADIUS,
        y: Math.sin(angle) * RADIUS,
        z: BASE_HEIGHT + Math.sin(angle * 2) * HEIGHT_AMPLITUDE,
    });

    ue.setRotation(actor, {
        pitch: 0,
        yaw: time * ROTATION_SPEED,
        roll: 0,
    });
}

export function dispose() {
    ue.log("[demo] dispose");
    if (actor) {
        ue.destroy(actor);
        actor = 0;
    }
}
